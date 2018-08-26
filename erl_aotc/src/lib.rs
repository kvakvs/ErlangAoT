extern crate erl_shared;
extern crate llvm_sys as llvm;

pub mod codegen;
pub mod ll_types;
mod fterm;
mod erlang_term;
pub mod aotc_main;


#[cfg(test)]
mod tests {
  use erlang_term::TermParser;
  use fterm::FTerm;


  #[test]
  fn erlang_term_parser_atoms() {
    let expr = TermParser::new().parse("atom").unwrap();
    assert_eq!(expr, FTerm::Atom("atom".to_string()));

    let expr = TermParser::new().parse("'aaa@example.com'").unwrap();
    assert_eq!(expr, FTerm::Atom("aaa@example.com".to_string()));
  }

  #[test]
  fn erlang_term_parser_str() {
    let expr = TermParser::new().parse(r#""""#).unwrap();
    assert_eq!(expr, FTerm::String(String::new()));

    let expr = TermParser::new().parse(r#""str""#).unwrap();
    assert_eq!(expr, FTerm::String("str".to_string()));
  }

  #[test]
  fn erlang_term_parser_list() {
    let expr = TermParser::new().parse("[atom]").unwrap();
    assert_eq!(expr, FTerm::List(vec![FTerm::Atom("atom".to_string())]));

    let expr = TermParser::new().parse("[atom, atom]").unwrap();
    assert_eq!(expr, FTerm::List(vec![FTerm::Atom("atom".to_string()),
                                      FTerm::Atom("atom".to_string())]));

    let expr = TermParser::new().parse("{atom}").unwrap();
    assert_eq!(expr, FTerm::Tuple(vec![FTerm::Atom("atom".to_string())]));

    let expr = TermParser::new().parse("{atom, atom}").unwrap();
    assert_eq!(expr, FTerm::Tuple(vec![FTerm::Atom("atom".to_string()),
                                       FTerm::Atom("atom".to_string())]));
  }
}
