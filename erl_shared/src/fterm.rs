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
