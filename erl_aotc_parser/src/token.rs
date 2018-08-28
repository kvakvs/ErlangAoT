#[derive(Clone, PartialEq, Debug)]
pub enum Token {
  StringLiteral(String),
  AtomLiteral(String),
  IntLiteral(i64),
  FloatLiteral(f64),
  Comment,

  Comma,
  Dot,
  LCurlyBracket,
  LSquareBracket,
  LParen,
  RCurlyBracket,
  RSquareBracket,
  RParen,
  Minus,
}
