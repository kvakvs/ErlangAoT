#[derive(Debug)]
pub struct MFA {
  m: String,
  f: String,
  a: usize,
}

impl MFA {
  pub fn new2(f: String, a: usize) -> MFA {
    MFA { m: String::new(), f, a }
  }

  pub fn new3(m: String, f: String, a: usize) -> MFA {
    MFA { m, f, a }
  }
}

#[derive(Debug)]
pub struct Module {
  name: String,
  imports: Vec<MFA>,
  exports: Vec<MFA>,
}

impl Module {
  pub fn new(name: String,
             imports: Vec<MFA>,
             exports: Vec<MFA>) -> Module {
    Module {
      name,
      imports,
      exports,
    }
  }
}
