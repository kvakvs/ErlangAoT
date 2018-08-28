extern crate erl_shared;

use erlang_term::DottedTermParser;
use erl_shared::fterm::FTerm;

pub mod erlang_term;
pub mod position;
mod token;
mod lexer;


pub fn parse(input: &str) -> FTerm {
    let lexr = lexer::Lexer::new(input);
    DottedTermParser::new().parse(lexr).unwrap()
}


#[cfg(test)]
mod tests {
  use erlang_term::TermParser;
  use erl_shared::fterm::FTerm;


  #[test]
    fn erlang_term_parser_atoms() {
        let expr = ::parse("atom");
        assert_eq!(expr, FTerm::Atom("atom".to_string()));

        let expr = ::parse("'aaa@example.com'");
        assert_eq!(expr, FTerm::Atom("aaa@example.com".to_string()));
    }

    #[test]
    fn erlang_term_parser_str() {
        let expr = ::parse(r#""""#);
        assert_eq!(expr, FTerm::String(String::new()));

        let expr = ::parse(r#""str""#);
        assert_eq!(expr, FTerm::String("str".to_string()));
    }

    #[test]
    fn erlang_term_parser_escaped_str() {
        let expr = ::parse(r#""\"""#);
        assert_eq!(expr, FTerm::String(r#"""#.to_string()));
    }

    #[test]
    fn erlang_term_parser_list() {
        let expr = ::parse("[atom]");
        assert_eq!(expr, FTerm::List(vec![FTerm::Atom("atom".to_string())]));

        let expr = ::parse("[atom, atom]");
        assert_eq!(expr, FTerm::List(vec![FTerm::Atom("atom".to_string()),
                                          FTerm::Atom("atom".to_string())]));

        let expr = ::parse("{atom}");
        assert_eq!(expr, FTerm::Tuple(vec![FTerm::Atom("atom".to_string())]));

        let expr = ::parse("{atom, atom}");
        assert_eq!(expr, FTerm::Tuple(vec![FTerm::Atom("atom".to_string()),
                                           FTerm::Atom("atom".to_string())]));
    }
}
