use std::fmt;

/// Represents Erlang values.
// #[repr(u8)]
#[derive(PartialEq, Clone)]
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
    panic!("Atom is expected, got {}", self)
  }

  pub fn int_val(&self) -> i64 {
    if let FTerm::Int64(i) = self {
      return *i;
    }
    panic!("Int64 is expected, got {}", self)
  }
}


impl fmt::Display for FTerm {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    match self {
      FTerm::Atom(s) => write!(f, "{}", s),
      FTerm::String(s) => write!(f, "\"{}\"", s),
      FTerm::Int64(i) => write!(f, "{}", i),
      FTerm::Float(flt) => write!(f, "{}", flt),
      FTerm::EmptyList => write!(f, "[]"),
      FTerm::List(v) => print_list(f, "[", "]", &v),
      FTerm::EmptyTuple => write!(f, "{{}}"),
      FTerm::Tuple(v) => print_list(f, "{", "}", &v),
    }
  }
}


fn print_list(f: &mut fmt::Formatter, open: &str, close: &str,
              vec: &Vec<FTerm>) -> fmt::Result {
  write!(f, "{}", open);
  let mut first = true;
  for ft in vec {
    if first {
      first = false;
    } else {
      write!(f, ", ");
    }
    write!(f, "{}", ft);
  }
  write!(f, "{}", close)
}
