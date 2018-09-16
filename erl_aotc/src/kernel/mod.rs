use erl_types::MFA;
use erl_shared::fterm::FTerm;
use std::collections::BTreeMap;

pub mod expr;
pub mod parse;


#[derive(Debug, Clone)]
pub enum Expr {
  // Logic and actions
  Match(Box<KMatch>),
  Seq(Box<KSeq>),
  Alt(KAlt),
  Enter(Box<KEnter>),
  Return(KReturn),
  Select(Box<KSelect>),
  Guard(KGuard),
  GuardBreak { anno: FTerm, args: Vec<Expr> },
  MultipleExprs(Vec<Expr>),
  Bif(Box<KCall>),
  Call(Box<KCall>),
  Put { anno: FTerm, arg: Box<Expr>, ret: Box<Expr> },
  Protected { anno: FTerm, arg: Box<Expr>, ret: Box<Expr> },
  Test { anno: FTerm, op: Box<FunRef>, args: Vec<Expr>, inverted: bool },
  GuardMatch(Box<KMatch>),

  // Values, literals, constructors and constants
  Atom(String),
  Int64(i64),
  Variable(String),
  Nil,
  Tuple { anno: FTerm, elements: Vec<Expr> },
  Value { anno: FTerm, val: FTerm },
  Cons { anno: FTerm, hd: Box<Expr>, tl: Box<Expr> },
  ConstructBinary {
    anno: FTerm,
    segments: Option<Box<KBinarySegment>>
  },
}


#[derive(Debug, Clone)]
pub struct KBinarySegment {
  pub anno: FTerm,
  pub size: Expr,
  pub unit: u32,
  pub seg_type: String, // TODO: type this
  pub flags: Vec<String>, // TODO: type this
  pub seg: Expr,
  pub next: Option<Box<KBinarySegment>>,
}


#[derive(Debug, Clone)]
pub enum FunRef {
  MFArity { m: Expr, f: Expr, arity: Expr },
  FArity { f: Expr, arity: Expr },
  Bif(Box<KCall>),
  Internal(MFA),
}


#[derive(Debug, Clone)]
pub struct KCall {
  pub anno: FTerm,
  pub op: FunRef,
  pub args: Vec<Expr>,
  pub ret: Vec<Expr>
}

impl FunRef {
//  pub fn get_mfa(&self) -> MFA {
//    match self {
//      FunRef::MFA(m) => m.clone(),
//      FunRef::Internal(m) => m.clone(),
//      _ => panic!("{:?} is not an FunRef::MFA", self),
//    }
//  }
}


/// Kernel Erlang k_match struct
#[derive(Debug, Clone)]
pub struct KMatch {
  pub anno: FTerm,
  pub vars: Vec<Expr>,
  pub body: Box<Expr>,
  pub ret: Expr,
}


#[derive(Debug, Clone)]
pub struct KAlt {
  pub anno: FTerm,
  pub first: Box<Expr>,
  pub then: Box<Expr>,
}


#[derive(Debug, Clone)]
pub struct KEnter {
  pub anno: FTerm,
  pub op: FunRef,
  pub args: Vec<Expr>,
}


#[derive(Debug, Clone)]
pub struct KReturn {
  pub anno: FTerm,
  pub args: Vec<Expr>,
}


#[derive(Debug, Clone)]
pub struct KSelect {
  pub anno: FTerm,
  pub var: Expr,
}


#[derive(Debug, Clone)]
pub struct KSeq {
  pub anno: FTerm,
  pub arg: Expr,
  pub body: Expr,
}


#[derive(Debug, Clone)]
pub struct KGuardClause {
  pub anno: FTerm,
  pub guard: Expr,
  pub body: Expr,
}


#[derive(Debug, Clone)]
pub struct KGuard {
  pub anno: FTerm,
  pub clauses: Vec<KGuardClause>,
}


#[derive(Debug)]
pub struct FunDef {
  pub funarity: MFA,
  k_code: Expr, // Kernel Code (parsed from Kernel Eterm input)
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
  pub fn new(name: String, arity: usize, k_code: Expr) -> FunDef {
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
