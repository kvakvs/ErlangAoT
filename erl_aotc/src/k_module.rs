use erl_module::{MFA, Module};
use erl_shared::fterm::FTerm;

/// Compile a parsed Kernel Module erlang term
pub fn process_module(mroot: FTerm) {
  // Step 1: Unwrap tuple with module elements and parse imports/exports
  let (mut mod1, fdefs) = match mroot {
    FTerm::Tuple(mdef) => process_module_1(mdef),
    _ => panic!("Expected: kernel module (k_mdef)")
  };
  println!("{:?}", mod1);
  // Step 2: Parse function definitions
  process_module_2(&mut mod1, fdefs);
}


/// Unwrap tuple with module elements and parse imports/exports
pub fn process_module_1(mdef: Vec<FTerm>) -> (Module, FTerm) {
  let m_imports = &mdef[1];
  let m_name = &mdef[2];
  let m_exports = &mdef[3];
  let m_attrs = &mdef[4];
  let m_fdefs = &mdef[5];

  let m = Module::new(m_name.atom_text(),
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
            2 => outp.push(MFA::new2(tvec[0].atom_text(),
                                     tvec[1].int_val() as usize)),
            3 => outp.push(MFA::new3(tvec[0].atom_text(),
                                     tvec[1].atom_text(),
                                     tvec[2].int_val() as usize)),
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


/// Parse function definitions.
fn process_module_2(m: &mut Module, fdefs: FTerm) {
  for fdef in fdefs.into_iter() {
    let fdef_vec = fdef.get_vec(); // anno, func, arity, attrs, body
    process_fun(&fdef_vec);
  }
}


fn process_fun(fdef_vec: &Vec<FTerm>) {
  let fname = &fdef_vec[2];
  let farity = &fdef_vec[3];
  let fattrs = &fdef_vec[4];
  let fbody = &fdef_vec[5];

  println!("------ {}/{} ------", fname, farity);
  println!("fn attrs={}", fattrs);

  let ops = process_code_block(fbody);
}


fn process_code_block(body: &FTerm) {
  let bvec = body.get_vec();
  if let FTerm::Atom(ref tag) = bvec[0] {
    match tag.as_ref() {
      "k_match" => process_kmatch(&bvec),
      "k_seq" => process_kseq(&bvec),
      "k_alt" => process_kalt(&bvec),
      "k_enter" => process_kenter(&bvec),
      "k_return" => process_kreturn(&bvec),
      "k_select" => process_kselect(&bvec),
      ref other => println!("-skip {}", other),
    }
  }
}


fn process_kmatch(kvec: &Vec<FTerm>) {
  // {k_match, anno, vars, body, ret}
  let body = &kvec[3];
  let _vars = &kvec[2];
  let ret = &kvec[4];
  println!("k_match -> {}", ret);
  process_code_block(body);
}


fn process_kseq(kvec: &Vec<FTerm>) {
  // {k_seq, anno, arg, body}
  let arg = &kvec[2];
  let body = &kvec[3];
  println!("k_seq -> {}", arg);
  process_code_block(body);
}


fn process_kenter(kvec: &Vec<FTerm>) {
  // {k_enter, anno, op, args}
  let op = &kvec[2];
  let args = &kvec[3];
  println!("k_enter {}({})", op, args);
}


fn process_kreturn(kvec: &Vec<FTerm>) {
  // {k_return, anno, args}
  let args = &kvec[2];
  println!("k_return -> {}", args);
}


fn process_kalt(kvec: &Vec<FTerm>) {
  // {k_alt, anno, first, then}
  let first = &kvec[2];
  let then = &kvec[3];
  println!("k_alt first {{");
  process_code_block(first);
  println!("}} k_alt then {{");
  process_code_block(then);
  println!("}}");
}

fn process_kselect(kvec: &Vec<FTerm>) {
  // {k_select, var, types}
  let var = &kvec[2];
  let types = &kvec[3];
  println!("k_select {} {}", var, types);
}
