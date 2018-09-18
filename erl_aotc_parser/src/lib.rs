extern crate erl_shared;

use erl_shared::fterm::FTerm;
use erlang_term::{DottedTermParser, TermParser};

pub mod erlang_term;
pub mod position;
mod token;
mod lexer;


pub fn parse(input: &str) -> FTerm {
  let lexr = lexer::Lexer::new(input);
  DottedTermParser::new().parse(lexr).unwrap()
}


pub fn parse_nodot(input: &str) -> FTerm {
  let lexr = lexer::Lexer::new(input);
  TermParser::new().parse(lexr).unwrap()
}


#[cfg(test)]
mod tests {
  use erl_shared::fterm::FTerm;


  fn mk_atom(s: &str) -> FTerm {
    FTerm::Atom(s.to_string())
  }


  #[test]
  fn erlang_term_parser_atom() {
    // One letter atom
    let expr = ::parse_nodot("a");
    assert_eq!(expr, mk_atom("a"));

    let expr = ::parse_nodot("atom");
    assert_eq!(expr, mk_atom("atom"));
  }

  #[test]
  fn erlang_term_parser_q_atom() {
    let expr = ::parse_nodot("'a'");
    assert_eq!(expr, mk_atom("a"));

    let expr = ::parse_nodot("'<='");
    assert_eq!(expr, mk_atom("<="));

    let expr = ::parse_nodot("'aaa@example.com'");
    assert_eq!(expr, mk_atom("aaa@example.com"));
  }

  #[test]
  fn erlang_term_parser_str() {
    let expr = ::parse_nodot(r#""""#);
    assert_eq!(expr, FTerm::String(String::new()));

    let expr = ::parse_nodot(r#""str""#);
    assert_eq!(expr, FTerm::String("str".to_string()));
  }

  #[test]
  fn erlang_term_parser_escaped_str() {
    let expr = ::parse_nodot(r#""\"""#);
    assert_eq!(expr, FTerm::String(r#"""#.to_string()));
  }

  #[test]
  fn erlang_term_parser_list() {
    let expr = ::parse_nodot("[atom]");
    assert_eq!(expr, FTerm::List(vec![mk_atom("atom")]));

    let expr = ::parse_nodot("[atom, atom]");
    assert_eq!(expr, FTerm::List(vec![mk_atom("atom"), mk_atom("atom")]));
  }

  #[test]
  fn erlang_term_parser_tuple() {
    let expr = ::parse_nodot("{atom}");
    assert_eq!(expr, FTerm::Tuple(vec![mk_atom("atom")]));

    let expr = ::parse_nodot("{atom, atom}");
    assert_eq!(expr, FTerm::Tuple(vec![mk_atom("atom"), mk_atom("atom")]));
  }
}
