use erl_module::{MFA, Module};
use erl_shared::fterm::FTerm;

/// Compile a parsed Kernel Module erlang term
pub fn process_module(mroot: FTerm) {
  match mroot {
    FTerm::Tuple(mdef) => {
      let m_imports = &mdef[1];
      let m_name = &mdef[2];
      let m_exports = &mdef[3];
      let m_attrs = &mdef[4];
      let m_fdefs = &mdef[5];
      println!("module {:?}", m_name);
      println!("exports {:?}", m_exports);

      let erlm = Module::new(m_name.atom_text(),
                             parse_mfa_list(m_imports),
                             parse_mfa_list(m_exports));
      println!("{:?}", erlm)
    }
    _ => panic!("Expected: kernel module (k_mdef)")
  };
  ()
}


/// Input: `FTerm::List[FTerm::Tuple{FTerm::Atom(fun),Fterm::I64(arity)}, ...]`
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
              panic!("FTerm::Tuple of 2 or 3 is expected, got {:?}", tvec)
          }
        } else {
          panic!("FTerm::Tuple of 2 or 3 is expected, got {:?}", lst)
        }
      }
      return outp;
    },
    FTerm::EmptyList => {
      return outp;
    },
    _ => { panic!("FTerm::List is expected, got {:?}", lst) },
  };
}
