use std::fmt;
//use erl_shared::fterm::FTerm;

#[derive(PartialOrd, PartialEq, Eq, Ord, Clone)]
pub struct MFA {
  pub m: String,
  pub f: String,
  pub a: usize,
}

impl MFA {
  pub fn new2(f: String, a: usize) -> MFA {
    MFA { m: String::new(), f, a }
  }

  pub fn new3(m: String, f: String, a: usize) -> MFA {
    MFA { m, f, a }
  }
}


impl fmt::Debug for MFA {
  #[inline]
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "{}", self)
  }
}


impl fmt::Display for MFA {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    if self.m.is_empty() {
      write!(f, "{}/{}", self.f, self.a)
    } else {
      write!(f, "{}:{}/{}", self.m, self.f, self.a)
    }
  }
}
