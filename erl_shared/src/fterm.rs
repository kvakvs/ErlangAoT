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
  Binary(Vec<u8>),
}

impl FTerm {
  pub fn get_atom_text(&self) -> String {
    if let FTerm::Atom(s) = self {
      return s.clone();
    }
    panic!("Atom is expected, got {}", self)
  }


  pub fn get_i64(&self) -> i64 {
    if let FTerm::Int64(i) = self {
      return *i;
    }
    panic!("Int64 is expected, got {}", self)
  }


  pub fn get_vec(&self) -> Vec<FTerm> {
    match self {
      FTerm::List(v) => v.clone(),
      FTerm::EmptyList => Vec::<FTerm>::new(),
      FTerm::Tuple(v) => v.clone(),
      FTerm::EmptyTuple => Vec::<FTerm>::new(),
      _ => panic!("Term must be a tuple or a list, got {}", self),
    }
  }


  pub fn is_int(&self) -> bool {
    match self {
      FTerm::Int64(_) => true,
      _ => false,
    }
  }


  pub fn is_atom(&self) -> bool {
    match self {
      FTerm::Atom(_) => true,
      _ => false,
    }
  }


  pub fn is_tuple(&self) -> bool {
    match self {
      FTerm::Tuple(_) => true,
      FTerm::EmptyTuple => true,
      _ => false,
    }
  }


  pub fn get_tuple_vec(&self) -> Vec<FTerm> {
    match self {
      FTerm::Tuple(v) => v.clone(),
      FTerm::EmptyTuple => Vec::<FTerm>::new(),
      _ => panic!("Term must be a tuple, got {}", self),
    }
  }


  pub fn get_bool(&self) -> bool {
    match self {
      FTerm::Atom(x) => x == "true",
      _ => panic!("Term must be a true|false atom, got {}", self),
    }
  }


  pub fn list_size(&self) -> usize {
    match self {
      FTerm::List(v) => v.len(),
      FTerm::EmptyList => 0,
      _ => panic!("Term must be a list, got {}", self),
    }
  }


  pub fn is_list(&self) -> bool {
    match self {
      FTerm::List(_) => true,
      FTerm::EmptyList => true,
      _ => false,
    }
  }


  pub fn get_list_vec(&self) -> Vec<FTerm> {
    match self {
      FTerm::List(v) => v.clone(),
      FTerm::EmptyList => Vec::<FTerm>::new(),
      _ => panic!("Term must be a list, got {}", self),
    }
  }


  pub fn is_atom_of(&self, s: &str) -> bool {
    match self {
      FTerm::Atom(s2) => s == s2,
      _ => false,
    }
  }
}


impl IntoIterator for FTerm {
  type Item = FTerm;
  type IntoIter = ::std::vec::IntoIter<FTerm>;

  fn into_iter(self) -> Self::IntoIter {
    match self {
      FTerm::List(v) => v.into_iter(),
      FTerm::Tuple(v) => v.into_iter(),
      _ => panic!("into_iter: Not iterable {}", self),
    }
  }
}


impl fmt::Debug for FTerm {
  #[inline]
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{}", self)
  }
}


/// Check whether a character belongs to an atom which can be printed without
/// enclosing it in 'quotes'. First character also cannot begin with a digit
/// but it is not checked here.
#[inline]
fn is_unquoted_atom_character(c: char) -> bool {
  c.is_ascii_alphanumeric() || c == '_'
}


#[inline]
fn is_first_unquoted_atom_character(c: char) -> bool {
  c.is_ascii_lowercase() || c.is_digit(10) || c == '_'
}


/// Check whether a string contains only characters that do not require enclosing
/// atom with 'single quotes'
#[inline]
fn is_unquoted_atom(s: &String) -> bool {
  if s.is_empty() { return false };

  let (_, first) = s.char_indices().next().unwrap();
  if !is_first_unquoted_atom_character(first) { return false };

  for (_, c) in s.char_indices() {
    if !is_unquoted_atom_character(c) { return false }
  }
  true
}


impl fmt::Display for FTerm {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    match self {
      FTerm::Atom(s) => {
        if is_unquoted_atom(s) { write!(f, "{}", s) }
        else { write!(f, "'{}'", s) }
      },
      FTerm::String(s) => {
        write!(f, "\"");
        for (_, c) in s.char_indices() {
          if c as usize <= 32usize { write!(f, "\\x{:x}", c as usize); }
          else { write!(f, "{}", c); }
        }
        write!(f, "\"")
      },
      FTerm::Int64(i) => write!(f, "{}", i),
      FTerm::Float(flt) => write!(f, "{}", flt),
      FTerm::EmptyList => write!(f, "[]"),
      FTerm::List(v) => print_list(f, "[", "]", &v),
      FTerm::EmptyTuple => write!(f, "{{}}"),
      FTerm::Tuple(v) => print_list(f, "{", "}", &v),
      FTerm::Binary(b) => write!(f, "<<{:?}>>", b),
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
