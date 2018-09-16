/// Parses Kernel Erlang input as Erlang Term in text.
/// Outputs a similar kernel::Kerl structure

use erl_shared::fterm::FTerm;
use erl_types::MFA;
use kernel::*;


/// Compile a parsed Kernel Module erlang term
pub fn process_module(mroot: FTerm) -> Module {
  // Step 1: Unwrap tuple with module elements and parse imports/exports
  let (mut mod1, fdefs) = match mroot {
    FTerm::Tuple(mdef) => create_kmod(mdef),
    _ => panic!("Expected: kernel module (k_mdef)")
  };
  println!("{:?}", mod1);
  // Step 2: Parse function definitions
  process_kmod_fdefs(mod1, fdefs)
}


/// Unwrap tuple with module elements and parse imports/exports
pub fn create_kmod(mdef: Vec<FTerm>) -> (Module, FTerm) {
  let m_imports = &mdef[1];
  let m_name = &mdef[2];
  let m_exports = &mdef[3];
  let m_attrs = &mdef[4];
  let m_fdefs = &mdef[5];

  let m = Module::new(m_name.get_atom_text(),
                      parse_mfa_list(m_imports),
                      parse_mfa_list(m_exports),
                      m_attrs.clone());
  (m, m_fdefs.clone())
}


/// Input: A list of m:f/a or f/a:
/// `FTerm::List[FTerm::Tuple{FTerm::Atom(fun),Fterm::I64(arity)}, ...]`
/// Returns: vector of MFA structs
fn parse_mfa_list(lst: &FTerm) -> Vec<MFA> {
  let mut outp = Vec::<MFA>::new();
  match lst {
    FTerm::List(vec) => {
      // For each pair or triple in list...
      for listeach in vec {
        if let FTerm::Tuple(tvec) = listeach {
          match tvec.len() {
            2 => outp.push(MFA::new2(tvec[0].get_atom_text(),
                                     tvec[1].get_i64() as usize)),
            3 => outp.push(MFA::new3(tvec[0].get_atom_text(),
                                     tvec[1].get_atom_text(),
                                     tvec[2].get_i64() as usize)),
            _ =>
              panic!("FTerm::Tuple of 2 or 3 is expected")
          }
        } else {
          panic!("FTerm::Tuple of 2 or 3 is expected, got {}", lst)
        }
      }
      return outp;
    }
    FTerm::EmptyList => {
      return outp;
    }
    _ => { panic!("FTerm::List is expected, got {}", lst) }
  };
}


/// Parse function definitions, return updated Kernel Module.
fn process_kmod_fdefs(mut kmod: Module, fdefs: FTerm) -> Module {
  for fdef in fdefs.into_iter() {
    // {k_fdef, anno, func, arity, attrs, body}
    let kfun = process_fun(&mut kmod, fdef.get_vec());
    kmod.add_fun(kfun)
  }
  kmod
}


fn process_fun(_kmod: &mut Module,
               fdef_vec: Vec<FTerm>) -> FunDef
{
  assert!(fdef_vec[0].is_atom_of("k_fdef"), "Expected 'f_def', got {:?}", fdef_vec);
  let fname = &fdef_vec[2];
  let farity = &fdef_vec[3];
  let fattrs = &fdef_vec[4];
  let fbody = &fdef_vec[5];

  println!("------ {}/{} ------", fname, farity);
  println!("fn attrs={} {{", fattrs);

  let k_code = parse_expr(0, &fbody);

  println!("}}");

  FunDef::new(fname.get_atom_text(),
              farity.get_i64() as usize,
              k_code)
}


const INDENT: u32 = 2;

fn ii(indent: u32) -> String {
  " ".to_string().repeat((indent * INDENT) as usize)
}


//fn parse_code(indent: u32, code_term: &FTerm) -> Expr {
//  if code_term.is_tuple() {
//    // Single op
//    return _parse_code_2(indent, code_term)
//  }
//  // List of ops
//  let mut result = Vec::with_capacity(code_term.list_size());
//  for op in code_term.get_vec() {
//    result.push(_parse_code_2(indent, &op))
//  }
//  KernlOp::MultipleOps(result)
//}


//fn _parse_code_2(indent: u32, code_term: &FTerm) -> KernlOp {
//  let ct = code_term.get_tuple_vec();
//  match ct[0].get_atom_text().as_ref() {
//    "k_match" => process_kmatch(indent + 1, &ct),
//    "k_seq" => parse_kseq(indent + 1, &code_term),
//    "k_alt" => process_kalt(indent + 1, &ct),
//    "k_enter" => process_kenter(indent + 1, &ct),
//    "k_return" => process_kreturn(indent + 1, &ct),
//    "k_select" => parse_kselect(indent + 1, &ct),
//    "k_guard" => parse_kguard(indent + 1, &ct),
//    "k_guard_break" => parse_kguard_break(indent + 1, &code_term),
//    ref other => panic!("---skip {}", other),
//  }
//}


fn parse_match(indent: u32, kmatch: &FTerm) -> Expr {
  // {k_match, anno, vars, body, ret}
  let match_vec = kmatch.get_tuple_vec();
  assert!(match_vec[0].is_atom_of("k_match"));

  let vars = &match_vec[2];
  let body = &match_vec[3];
  let ret = &match_vec[4];
  println!("{}k_match {} -> ret {} {{", ii(indent), vars, ret);
  let body = Box::new(parse_expr(indent + 1, body));
  println!("{}}} % end match", ii(indent));

  let km = KMatch {
    anno: match_vec[1].clone(),
    vars: parse_expr_list(indent, vars),
    body,
    ret: parse_ret(ret),
  };
  Expr::Match(Box::new(km))
}


fn parse_expr_list(indent: u32, vars: &FTerm) -> Vec<Expr> {
  if !vars.is_list() {
    panic!("List of something is expected, found {}", vars)
  }
  let mut result = Vec::<Expr>::new();
  for v in vars.get_list_vec() {
    result.push(_parse_expr_2(indent, &v))
  }
  result
}


fn parse_expr(indent: u32, expr: &FTerm) -> Expr {
  if expr.is_int() {
    return Expr::Int64(expr.get_i64())
  } else if expr.is_atom() {
    return Expr::Atom(expr.get_atom_text())
  } else if expr.is_list() {
    return Expr::MultipleExprs(parse_expr_list(indent, expr))
  }
  return _parse_expr_2(indent, expr)
}


fn _parse_expr_2(indent: u32, expr: &FTerm) -> Expr {
  // So val is a tuple, parse it as a k_* tuple or something
  let vvec = expr.get_tuple_vec();
  match vvec[0].get_atom_text().as_ref() {
    "k_var" => {
      // {k_var, anno, name}
      assert!(vvec[0].is_atom_of("k_var"));
      match &vvec[2] {
        FTerm::Atom(s) => Expr::Variable(s.to_string()),
        FTerm::Int64(i) => Expr::Variable(i.to_string()),
        _ => panic!("Don't know how to parse val {}", vvec[2]),
      }
    },
    "k_bif" => Expr::Bif(Box::new(parse_kcall(indent, vvec))),
    "k_atom" => { // {k_atom, anno, val}
      Expr::Atom(vvec[2].get_atom_text())
    },
    "k_int" => { // {k_int, anno, val}
      Expr::Int64(vvec[2].get_i64())
    },
    "k_call" => Expr::Call(Box::new(parse_kcall(indent, vvec))),
    "k_literal" => // {k_literal, anno, val}
      Expr::Value {
        anno: vvec[1].clone(),
        val: vvec[2].clone(),
      },
    "k_put" => {
      Expr::Put {
        anno: vvec[1].clone(),
        arg: Box::new(parse_expr(indent, &vvec[2])),
        ret: Box::new(parse_expr(indent, &vvec[3])),
      }
    },
    "k_cons" => {
      Expr::Cons {
        anno: vvec[1].clone(),
        hd: Box::new(parse_expr(indent, &vvec[2])),
        tl: Box::new(parse_expr(indent, &vvec[3])),
      }
    },
    "k_nil" => Expr::Nil,
    "k_protected" => { // {k_protected, anno, arg, ret}
      Expr::Protected {
        anno: vvec[1].clone(),
        arg: Box::new(parse_expr(indent, &vvec[2])),
        ret: Box::new(parse_expr(indent, &vvec[3])),
      }
    },
    "k_test" => {
      Expr::Test {
        anno: vvec[1].clone(),
        op: Box::new(parse_funref(indent, &vvec[2])),
        args: parse_expr_list(indent, &vvec[3]),
        inverted: vvec[4].get_bool(),
      }
    },
    "k_guard_match" => { // {k_guard_match, anno, vars, body, ret}
      let km = Box::new(KMatch {
        anno: vvec[1].clone(),
        vars: parse_expr_list(indent, &vvec[2]),
        body: Box::new(parse_expr(indent+1, &vvec[3])),
        ret: parse_expr(indent, &vvec[4]),
      });
      Expr::GuardMatch(km)
    },
    "k_tuple" => { // {k_tuple, anno, elements}
      Expr::Tuple {
        anno: vvec[1].clone(),
        elements: parse_expr_list(indent, &vvec[2]),
      }
    },
    "k_match" => parse_match(indent + 1, &expr),
    "k_seq" => parse_seq(indent + 1, &expr),
    "k_alt" => parse_alt(indent + 1, &expr),
    "k_enter" => parse_enter(indent + 1, &expr),
    "k_return" => parse_return(indent + 1, &expr),
    "k_select" => parse_select(indent + 1, &expr),
    "k_guard" => parse_guard(indent + 1, &expr),
    "k_guard_break" => parse_kguard_break(indent + 1, &expr),

    // TODO: k_enter
    // TODO: k_try, k_try_enter
    // TODO: k_catch
    // TODO: k_receive, k_receive_accept, k_receive_next
    // TODO: k_break

    _other => panic!("_parse_expr_2 doesn't know how to handle {} in {:?}",
                     vvec[0], vvec)
  }
}


fn parse_ret(ret: &FTerm) -> Expr {
  match ret {
    FTerm::EmptyList => Expr::Nil,
    _other => panic!("TODO parse_ret for {}", ret),
  }
}


fn parse_seq(indent: u32, kseq: &FTerm) -> Expr {
  // {k_seq, anno, arg, body}
  let seq_vec = kseq.get_tuple_vec();
  assert!(seq_vec[0].is_atom_of("k_seq"));

  let arg = &seq_vec[2];
  println!("{}k_seq -> {}", ii(indent), arg);

  let ks = KSeq {
    anno: seq_vec[1].clone(),
    arg: parse_expr(indent, arg),
    body: parse_expr(indent + 1, &seq_vec[3]),
  };
  Expr::Seq(Box::new(ks))
}


fn parse_enter(indent: u32, enter: &FTerm) -> Expr {
  // {k_enter, anno, op, args}
  let enter_vec = enter.get_tuple_vec();
  assert!(enter_vec[0].is_atom_of("k_enter"));

  let op = &enter_vec[2];
  let args = &enter_vec[3];
  println!("{}k_enter {}({})", ii(indent), op, args);

  let ke = KEnter {
    anno: enter_vec[1].clone(),
    op: parse_funref(indent, &op),
    args: parse_expr_list(indent, args),
  };
  Expr::Enter(Box::new(ke))
}


fn parse_funref(indent: u32, funref: &FTerm) -> FunRef {
  let kvec = funref.get_tuple_vec();
  let tag = kvec[0].get_atom_text();
  match tag.as_ref() {
    "k_local" => { // {k_local, anno, name, arity}
      return FunRef::FArity {
        f: parse_expr(indent, &kvec[2]),
        arity: parse_expr(indent, &kvec[3])
      }
    },
    "k_remote" => { // {k_remote, anno, mod, name, arity}
      return FunRef::MFArity {
        m: parse_expr(indent, &kvec[2]),
        f: parse_expr(indent, &kvec[3]),
        arity: parse_expr(indent, &kvec[4])
      }
    },
    "k_internal" => { // {k_internal, anno, name, arity}
      return FunRef::Internal(parse_kinternal(kvec))
    },
    "k_bif" => { // {k_bif, anno, op, args, ret=[]}
      return FunRef::Bif(
        Box::new(parse_kcall(indent, kvec))
      )
    }
    other => panic!("Don't know how to parse fun ref {}", other)
  }
}


fn parse_kinternal(kvec: Vec<FTerm>) -> MFA {
  // {k_internal, anno, name, arity}
  MFA::new2(kvec[2].get_atom_text(), kvec[3].get_i64() as usize)
}


fn parse_kcall(indent: u32, kvec: Vec<FTerm>) -> KCall {
  // {k_bif, anno, op, args, ret=[]}
  // {k_call, anno, op, args, ret}
  let op_mfa= parse_funref(indent, &kvec[2]);
  KCall {
    anno: kvec[1].clone(),
    op: op_mfa,
    args: parse_expr_list(indent, &kvec[3]),
    ret: parse_expr_list(indent, &kvec[4]),
  }
}


fn parse_return(indent: u32, ret: &FTerm) -> Expr {
  // {k_return, anno, args}
  let ret_vec = ret.get_tuple_vec();
  assert!(ret_vec[0].is_atom_of("k_return"));

  let args = &ret_vec[2];
  println!("{}k_return -> {}", ii(indent), args);

  let kret = KReturn {
    anno: ret_vec[1].clone(),
    args: parse_expr_list(indent, args),
  };
  Expr::Return(kret)
}


fn parse_alt(indent: u32, alt: &FTerm) -> Expr {
  // {k_alt, anno, first, then}
  let alt_vec = alt.get_tuple_vec();
  assert!(alt_vec[0].is_atom_of("k_alt"));

  println!("{}k_alt first {{", ii(indent));
  let first = &alt_vec[2];
  let kfirst = Box::new(
    parse_expr(indent + 1, first)
  );

  println!("{}}} k_alt then {{", ii(indent));
  let then = &alt_vec[3];
  let kthen = Box::new(parse_expr(indent + 1, then));
  println!("{}}} % end alt", ii(indent));

  let ka = KAlt {
    anno: alt_vec[1].clone(),
    first: kfirst,
    then: kthen,
  };
  Expr::Alt(ka)
}

fn parse_select(indent: u32, sel: &FTerm) -> Expr {
  // Assert kselect contains only type_clauses
  // {k_select, var, types}
  let sel_vec = sel.get_tuple_vec();
  assert!(sel_vec[0].is_atom_of("k_select"));

  let var = &sel_vec[2];
  let type_clauses = &sel_vec[3];
  println!("{}k_select {} {{", ii(indent), var);
  parse_ktype_clauses(indent + 1, type_clauses.get_vec());
  println!("{}}} % end select", ii(indent));

  let ks = KSelect {
    anno: sel_vec[1].clone(),
    var: parse_expr(indent, &var),

  };
  Expr::Select(Box::new(ks))
}


fn parse_ktype_clauses(indent: u32, tclauses: Vec<FTerm>) {
  for tclause in tclauses {
    let tclause_vec = tclause.get_vec();
    // {k_type_clause, anno, type, values}
    assert!(tclause_vec[0].is_atom_of("k_type_clause"));

    let typeclause_type = &tclause_vec[2];
    let typeclause_valclauses = &tclause_vec[3];

    println!("{}k_type_clause {} {{", ii(indent), typeclause_type);
    parse_kval_clauses(indent + 1, typeclause_valclauses.get_vec());
    println!("{}}}", ii(indent))
  }
}


fn parse_kval_clauses(indent: u32, vclauses: Vec<FTerm>) {
  for vc in vclauses {
    // {k_val_clause, anno, val, body}
    let vclause_vec = vc.get_vec();
    assert!(vclause_vec[0].is_atom_of("k_val_clause"));

    let vclause_val = &vclause_vec[2];
    let _vclause_body = &vclause_vec[3];
    println!("{}k_val_clause {}", ii(indent), vclause_val)
  }
}


fn parse_guard(indent: u32, guard: &FTerm) -> Expr {
  // {k_guard, anno, clauses}
  let guard_vec = guard.get_tuple_vec();
  assert!(guard_vec[0].is_atom_of("k_guard"));

  let gclauses_term = &guard_vec[2];
  let mut clauses = Vec::<KGuardClause>::new();
  for gclause in gclauses_term.get_list_vec() {
    println!("{}k_guard {{", ii(indent));
    let clause = parse_kguard_clauses(indent + 1, &gclause);
    clauses.push(clause);
    println!("{}}} % end guard", ii(indent));
  }
  let kg = KGuard {
    anno: guard_vec[1].clone(),
    clauses
  };
  Expr::Guard(kg)
}


fn parse_kguard_clauses(indent: u32, kgc_tuple: &FTerm) -> KGuardClause {
  assert!(kgc_tuple.is_tuple());
  let v = kgc_tuple.get_tuple_vec();
  assert!(v[0].is_atom_of("k_guard_clause"));

  println!("{}kguardclause {{", ii(indent));
  let body = parse_expr(indent + 1, &v[3]);
  println!("{}}}", ii(indent));

  KGuardClause {
    anno: v[1].clone(),
    guard: parse_expr(indent, &v[2]),
    body,
  }
}


fn parse_kguard_break(indent: u32, k_gb: &FTerm) -> Expr {
  // {k_guard_break, anno, args}
  let gbvec = k_gb.get_tuple_vec();
  assert!(gbvec[0].is_atom_of("k_guard_break"));

  let args = parse_expr_list(indent, &gbvec[2]);
  println!("{}kguard_break {:?}", ii(indent), args);
  Expr::GuardBreak {
    anno: gbvec[1].clone(),
    args,
  }
}
