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


fn process_fun(kmod: &mut Module, fdef_vec: Vec<FTerm>) -> FunDef {
  assert!(fdef_vec[0].is_atom_of("k_fdef"), "Expected 'f_def', got {:?}", fdef_vec);
  let fname = &fdef_vec[2];
  let farity = &fdef_vec[3];
  let fattrs = &fdef_vec[4];
  let fbody = &fdef_vec[5];

  println!("------ {}/{} ------", fname, farity);
  println!("fn attrs={} {{", fattrs);

  let k_code = process_code_block(0, fbody.get_vec());

  println!("}}");

  FunDef::new(fname.get_atom_text(),
              farity.get_i64() as usize,
              k_code)
}


const INDENT: u32 = 2;

fn ii(indent: u32) -> String {
  " ".to_string().repeat((indent * INDENT) as usize)
}


fn process_code_block(indent: u32, code_term: Vec<FTerm>) -> KernlOp {
  match code_term[0].get_atom_text().as_ref() {
    "k_match" => process_kmatch(indent + 1, &code_term),
    "k_seq" => process_kseq(indent + 1, &code_term),
    "k_alt" => process_kalt(indent + 1, &code_term),
    "k_enter" => process_kenter(indent + 1, &code_term),
    "k_return" => process_kreturn(indent + 1, &code_term),
    "k_select" => process_kselect(indent + 1, &code_term),
    "k_guard" => process_kguard(indent + 1, &code_term),
    ref other => panic!("{} -skip {}", ii(indent), other),
  }
}


fn process_kmatch(indent: u32, kmatch: &Vec<FTerm>) -> KernlOp {
  // {k_match, anno, vars, body, ret}
  assert!(kmatch[0].is_atom_of("k_match"));
  let vars = &kmatch[2];
  let body = &kmatch[3];
  let ret = &kmatch[4];
  println!("{}k_match {} -> ret {} {{", ii(indent), vars, ret);
  let body = Box::new(
    process_code_block(indent + 1, body.get_vec())
  );
  println!("{}}} % end match", ii(indent));

  let km = KMatch {
    anno: kmatch[1].clone(),
    vars: parse_expr_list(vars),
    body,
    ret: parse_ret(ret),
  };
  KernlOp::Match(km)
}


fn parse_expr_list(vars: &FTerm) -> Vec<Expr> {
  if !vars.is_list() {
    panic!("List of something is expected, found {}", vars)
  }
  let mut result = Vec::<Expr>::new();
  for v in vars.get_list_vec() {
    result.push(_parse_expr_2(&v))
  }
  result
}


fn parse_expr(expr: &FTerm) -> Expr {
  if expr.is_int() {
    return Expr::Int64(expr.get_i64())
  } else if expr.is_atom() {
    return Expr::Atom(expr.get_atom_text())
  } else if expr.is_list() {
    return Expr::MultipleExprs(parse_expr_list(expr))
  }
  return _parse_expr_2(expr)
}


fn _parse_expr_2(expr: &FTerm) -> Expr {
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
    "k_bif" => Expr::Bif(Box::new(parse_kcall(vvec))),
    "k_atom" => { // {k_atom, anno, val}
      Expr::Atom(vvec[2].get_atom_text())
    },
    "k_int" => { // {k_int, anno, val}
      Expr::Int64(vvec[2].get_i64())
    },
    "k_call" => Expr::Call(Box::new(parse_kcall(vvec))),
    "k_literal" => // {k_literal, anno, val}
      Expr::Literal {
        anno: vvec[1].clone(),
        val: vvec[2].clone()
      },
    "k_put" => {
      
    },
    // TODO: k_test
    // TODO: k_enter
    // TODO: k_match
    // TODO: k_guard_match
    // TODO: k_try, k_try_enter
    // TODO: k_catch
    // TODO: k_receive, k_receive_accept, k_receive_next
    // TODO: k_break
    // TODO: k_guard_break
    // TODO: k_return

    other => panic!("_parse_expr_2 doesn't know how to handle {} in {:?}",
                    vvec[0], vvec)
  }
}


fn parse_ret(ret: &FTerm) -> Expr {
  match ret {
    FTerm::EmptyList => Expr::Nil,
    other => panic!("TODO parse_ret for {}", ret),
  }
}


fn process_kseq(indent: u32, kseq: &Vec<FTerm>) -> KernlOp {
  // {k_seq, anno, arg, body}
  assert!(kseq[0].is_atom_of("k_seq"));
  let arg = &kseq[2];
  let body = &kseq[3];
  println!("{}k_seq -> {}", ii(indent), arg);
  let k_code = Box::new(
    process_code_block(indent + 1, body.get_vec())
  );

  let ks = KSeq {
    anno: kseq[1].clone(),
    arg: parse_expr(arg),
    body: k_code,
  };
  KernlOp::Seq(ks)
}


fn process_kenter(indent: u32, kenter: &Vec<FTerm>) -> KernlOp {
  // {k_enter, anno, op, args}
  assert!(kenter[0].is_atom_of("k_enter"));
  let op = &kenter[2];
  let args = &kenter[3];
  println!("{}k_enter {}({})", ii(indent), op, args);

  let ke = KEnter {
    anno: kenter[1].clone(),
    op: parse_funref(op.get_vec()),
    args: parse_expr_list(args),
  };
  KernlOp::Enter(ke)
}


fn parse_funref(kvec: Vec<FTerm>) -> FunRef {
  let tag = kvec[0].get_atom_text();
  match tag.as_ref() {
    "k_local" => { // {k_local, anno, name, arity}
      return FunRef::FArity {
        f: parse_expr(&kvec[2]),
        arity: parse_expr(&kvec[3])
      }
    },
    "k_remote" => { // {k_remote, anno, mod, name, arity}
      return FunRef::MFArity {
        m: parse_expr(&kvec[2]),
        f: parse_expr(&kvec[3]),
        arity: parse_expr(&kvec[4])
      }
    },
    "k_internal" => { // {k_internal, anno, name, arity}
      return FunRef::Internal(parse_kinternal(kvec))
    },
    "k_bif" => { // {k_bif, anno, op, args, ret=[]}
      return FunRef::Bif(
        Box::new(parse_kcall(kvec))
      )
    }
    other => panic!("Don't know how to parse fun ref {}", other)
  }
}


fn parse_kinternal(kvec: Vec<FTerm>) -> MFA {
  // {k_internal, anno, name, arity}
  MFA::new2(kvec[2].get_atom_text(), kvec[3].get_i64() as usize)
}


fn parse_kcall(kvec: Vec<FTerm>) -> KCall {
  // {k_bif, anno, op, args, ret=[]}
  // {k_call, anno, op, args, ret}
  let op_mfa= parse_funref(kvec[2].get_vec());
  KCall {
    anno: kvec[1].clone(),
    op: op_mfa,
    args: parse_expr_list(&kvec[3]),
    ret: parse_expr_list(&kvec[4]),
  }
}


fn process_kreturn(indent: u32, kreturn: &Vec<FTerm>) -> KernlOp {
  // {k_return, anno, args}
  assert!(kreturn[0].is_atom_of("k_return"));
  let args = &kreturn[2];
  println!("{}k_return -> {}", ii(indent), args);

  let kret = KReturn {
    anno: kreturn[1].clone(),
    args: parse_expr_list(args),
  };
  KernlOp::Return(kret)
}


fn process_kalt(indent: u32, kalt: &Vec<FTerm>) -> KernlOp {
  // {k_alt, anno, first, then}
  assert!(kalt[0].is_atom_of("k_alt"));

  println!("{}k_alt first {{", ii(indent));
  let first = &kalt[2];
  let kfirst = Box::new(
    process_code_block(indent + 1, first.get_vec())
  );

  println!("{}}} k_alt then {{", ii(indent));
  let then = &kalt[3];
  let kthen = Box::new(
    process_code_block(indent + 1, then.get_vec())
  );
  println!("{}}} % end alt", ii(indent));

  let ka = KAlt {
    anno: kalt[1].clone(),
    first: kfirst,
    then: kthen,
  };
  KernlOp::Alt(ka)
}

fn process_kselect(indent: u32, kselect: &Vec<FTerm>) -> KernlOp {
  // Assert kselect contains only type_clauses
  // {k_select, var, types}
  assert!(kselect[0].is_atom_of("k_select"));
  let var = &kselect[2];
  let type_clauses = &kselect[3];
  println!("{}k_select {} {{", ii(indent), var);
  process_k_type_clauses(indent + 1, type_clauses.get_vec());
  println!("{}}} % end select", ii(indent));

  let ks = KSelect {
    anno: kselect[1].clone(),
    var: parse_expr(&var),

  };
  KernlOp::Select(ks)
}


fn process_k_type_clauses(indent: u32, tclauses: Vec<FTerm>) {
  for tclause in tclauses {
    let tclause_vec = tclause.get_vec();
    // {k_type_clause, anno, type, values}
    assert!(tclause_vec[0].is_atom_of("k_type_clause"));

    let typeclause_type = &tclause_vec[2];
    let typeclause_valclauses = &tclause_vec[3];

    println!("{}k_type_clause {} {{", ii(indent), typeclause_type);
    process_k_val_clauses(indent + 1, typeclause_valclauses.get_vec());
    println!("{}}}", ii(indent))
  }
}


fn process_k_val_clauses(indent: u32, vclauses: Vec<FTerm>) {
  for vc in vclauses {
    // {k_val_clause, anno, val, body}
    let vclause_vec = vc.get_vec();
    assert!(vclause_vec[0].is_atom_of("k_val_clause"));

    let vclause_val = &vclause_vec[2];
    let vclause_body = &vclause_vec[3];
    println!("{}k_val_clause {}", ii(indent), vclause_val)
  }
}


fn process_kguard(indent: u32, kguard: &Vec<FTerm>) -> KernlOp {
  // {k_guard, anno, clauses}
  assert!(kguard[0].is_atom_of("k_guard"));
  let guard_clauses = &kguard[2];
  let guard_clauses_vec = guard_clauses.get_vec();

  let mut clauses = Vec::<Box<KernlOp>>::new();
  if !guard_clauses_vec.is_empty() {
    println!("{}k_guard {{", ii(indent));
    let clause = Box::new(
      process_code_block(indent + 1, guard_clauses_vec)
    );
    clauses.push(clause);
    println!("{}}} % end guard", ii(indent));
  }
  let kg = KGuard {
    anno: kguard[1].clone(),
    clauses
  };
  KernlOp::Guard(kg)
}
