use erl_types::MFA;
use erl_shared::fterm::FTerm;
use std::collections::BTreeMap;


#[derive(Debug, Clone)]
pub enum Value {
  Variable(String),
  Nil,
  Literal(FTerm),
}


#[derive(Debug, Clone)]
pub enum FunRef {
  Local(String, usize),
  External(String, String, usize),
}


/// Kernel Erlang k_match struct
#[derive(Debug, Clone)]
pub struct KMatch {
  pub anno: FTerm,
  pub vars: Vec<Value>,
  pub body: Box<KernlOp>,
  pub ret: Value,
}


#[derive(Debug, Clone)]
pub struct KAlt {
  pub anno: FTerm,
  pub first: Box<KernlOp>,
  pub then: Box<KernlOp>,
}


#[derive(Debug, Clone)]
pub struct KEnter {
  pub anno: FTerm,
  pub op: FunRef,
  pub args: Vec<Value>,
}


#[derive(Debug, Clone)]
pub struct KReturn {
  pub anno: FTerm,
  pub args: Vec<Value>,
}


#[derive(Debug, Clone)]
pub struct KSelect {
  pub anno: FTerm,
  pub var: Value,
}


#[derive(Debug, Clone)]
pub struct KSeq {
  pub anno: FTerm,
  pub arg: Value,
  pub body: Box<KernlOp>,
}


#[derive(Debug, Clone)]
pub struct KGuard {
  pub anno: FTerm,
  pub clauses: Vec<Box<KernlOp>>,
}


#[derive(Debug, Clone)]
pub enum KernlOp {
  Match(KMatch),
  Seq(KSeq),
  Alt(KAlt),
  Enter(KEnter),
  Return(KReturn),
  Select(KSelect),
  Guard(KGuard),
}


impl KernlOp {
  pub fn kmatch(k: &KernlOp) -> KMatch {
    match k {
      KernlOp::Match(x) => x.clone(),
      _ => panic!("KernlOp {:?} is not a Match()", k)
    }
  }
}


#[derive(Debug)]
pub struct FunDef {
  pub funarity: MFA,
  k_code: KernlOp, // Kernel Code (parsed from Kernel Eterm input)
}


#[derive(Debug)]
pub struct Module {
  name: String,
  imports: Vec<MFA>,
  exports: Vec<MFA>,
  attrs: FTerm,
  funs: BTreeMap<MFA, FunDef>
}


impl FunDef {
  pub fn new(name: String, arity: usize, k_code: KernlOp) -> FunDef {
    FunDef {
      funarity: MFA::new2(name, arity),
      k_code
    }
  }
}


impl Module {
  pub fn new(name: String,
             imports: Vec<MFA>,
             exports: Vec<MFA>,
             attrs: FTerm) -> Module {
    Module {
      name,
      imports,
      exports,
      attrs,
      funs: BTreeMap::new(),
    }
  }


  pub fn add_fun(&mut self, fdef: FunDef) {
    let fa = fdef.funarity.clone();
    self.funs.insert(fa, fdef);
  }
}
