/// Represents Erlang values.
// #[repr(u8)]
#[derive(Debug, PartialEq, Clone)]
#[allow(dead_code)]
pub enum FTerm {
  /// Runtime atom index in the VM atom table
  Atom(String),
  String(String),
  Int64(i64),
  List(Vec<FTerm>),
  EmptyList,
  Tuple(Vec<FTerm>),
  EmptyTuple,
  Float(f64),
}

impl FTerm {
  pub fn atom_text(&self) -> String {
    if let FTerm::Atom(s) = self {
      return s.clone();
    }
    panic!("Atom is expected, got {:?}", self)
  }

  pub fn int_val(&self) -> i64 {
    if let FTerm::Int64(i) = self {
      return *i;
    }
    panic!("Int64 is expected, got {:?}", self)
  }
}
