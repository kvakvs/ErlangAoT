use position::Position;
use std::str::CharIndices;
use token::Token;

#[derive(Debug, Copy, Clone)]
pub enum LexicalError {
  UnterminatedStringLiteral,
  UnexpectedEscapeCode(char),
  UnexpectedEndOfFile,
}


pub type Spanned<Token, Loc, Error> = Result<(Loc, Token, Loc), Error>;

pub struct SpannedToken {
  start: Position,
  tok: Token,
  end: Position,
}


pub struct Lexer<'input> {
  // Current line (position) in the input for error reporting
  current_pos: Position,
  input_iter: CharIndices<'input>,
  // This might become None anytime when the input ends
  next: Option<(usize, char)>,
}


impl<'input> Lexer<'input> {
  pub fn new(input: &'input str) -> Self {
    let mut input_iter = input.char_indices();
    // Create iterator on input and step 1 forward immediately
    // Save next character for lookahead
    let next = input_iter.next();
    Lexer {
      input_iter,
      current_pos: Position::new(),
      next,
    }
  }


  fn string_literal(&mut self) -> Result<SpannedToken, LexicalError> {
    let start = self.current_pos;
    let mut out_str = String::new();
    out_str.reserve(32);

    while let Some((nxt_pos, ch)) = self.my_next() {
      match ch {
        '\\' => out_str.push(self.escape_code()?),
        '"' => {
          match self.my_next() {
            Some((end, _end_ch)) => {
              let token = Token::StringLiteral(out_str);
              return Ok(SpannedToken {
                start,
                tok: token,
                end: start, // end position same as start (because same line)
              });
            }
            None => return Err(LexicalError::UnexpectedEndOfFile),
          }
        }, // match my_next
        ch => out_str.push(ch),
      } // while let some
    }

    Err(LexicalError::UnterminatedStringLiteral)
  }


  /// Steps one forward with `self.input_iterator` and updates `next`, returning
  /// you the previous `next`
  fn my_next(&mut self) -> Option<(usize, char)> {
    let old_next = self.next;
    self.next = self.input_iter.next();
    old_next
  }


  fn escape_code(&mut self) -> Result<char, LexicalError> {
    match self.my_next() {
      Some((_, ch)) => {
        match ch {
          '\'' => Ok('\''),
          '"' => Ok('"'),
          '\\' => Ok('\\'),
          '/' => Ok('/'),
          'n' => Ok('\n'),
          'r' => Ok('\r'),
          't' => Ok('\t'),
          other => Err(LexicalError::UnexpectedEscapeCode(other)),
        }
      },
      None => Err(LexicalError::UnexpectedEndOfFile),
    }
  }

  fn make_spannedtok(&self, t: Token) -> SpannedToken {
    SpannedToken {
      start: Position::new(),
      end: Position::new(),
      tok: t,
    }
  }
}

impl<'input> Iterator for Lexer<'input> {
  //type Item = Spanned<Token, usize, LexicalError>;
  type Item = Result<SpannedToken, LexicalError>;

  fn next(&mut self) -> Option<Self::Item> {
    loop {
      match self.my_next() {
        Some((i, ch)) => {
          match ch {
            '[' => return Some(Ok(self.make_spannedtok(Token::LSquareBracket))),
            '.' => return Some(Ok(self.make_spannedtok(Token::Dot))),
            '{' => return Some(Ok(self.make_spannedtok(Token::LCurlyBracket))),
            '(' => return Some(Ok(self.make_spannedtok(Token::LParen))),
            ']' => return Some(Ok(self.make_spannedtok(Token::RSquareBracket))),
            '}' => return Some(Ok(self.make_spannedtok(Token::RCurlyBracket))),
            ')' => return Some(Ok(self.make_spannedtok(Token::RParen))),
            '-' => return Some(Ok(self.make_spannedtok(Token::Minus))),
            ',' => return Some(Ok(self.make_spannedtok(Token::Comma))),

            '"' => return Some(self.string_literal()),
            //'\'' => Some(self.char_literal(start)),
            //ch if ch.is_digit(10) => Some(self.numeric_literal(start)),

//        ch if is_digit(ch)
//            || (ch == '-' && self.test_lookahead(is_digit)) => {
//          Some(self.numeric_literal(start))
//        }
            _ => continue, // Comment; skip this character
          }
        },
        None => return None,
      } // match next
    } // loop
  } // end next
}
