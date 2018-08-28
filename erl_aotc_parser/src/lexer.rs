// for numeric types ::from_str()
use position::Position;
use std::str::CharIndices;
use std::str::FromStr;
use token::Token;

#[derive(Debug, Copy, Clone)]
pub enum LexicalError {
  UnterminatedStringLiteral,
  UnexpectedEscapeCode(char),
  UnexpectedEndOfFile,
  Unexpected(char),
}


pub type Spanned<Token, Loc, Error> = Result<(Loc, Token, Loc), Error>;

pub type SpannedToken = (usize, Token, usize);

type InputChar = (usize, char);

pub struct Lexer<'input> {
  // Input character index (not very useful for error reporting)
  current_index: usize,
  input: CharIndices<'input>,

  // This contains 0, 1 or more InputChar's consumed from input.
  // Lazily populated one by one, or when an InputChar is unread
  upcoming: Vec<InputChar>,
  // This contains already consumed InputChar's
  past: Vec<InputChar>,
}


impl<'input> Lexer<'input> {
  fn err<T>(&self, e: LexicalError) -> Result<T, LexicalError> {
    Err(e)
  }

  pub fn new(input_str: &'input str) -> Self {
    let mut input = input_str.char_indices();

    // Save next character for lookahead (if any input exists)
    let mut upc = Vec::<InputChar>::new();
    if let Some(next) = input.next() {
      upc.push(next)
    };

    Lexer {
      input,
      current_index: 0,
      upcoming: upc,
      past: Vec::new(),
    }
  }


  fn string_literal(&mut self) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(32);

    let start = self.current_index;

    while let Some((pos, ch)) = self.consume() {
      match ch {
        '\\' => out_str.push(self.escape_code()?),
        '"' => return Ok((start, Token::StringLiteral(out_str), pos)),
        ch => out_str.push(ch),
      } // match my_next
    } // while let some

    Err(LexicalError::UnterminatedStringLiteral)
  }


  fn quoted_atom_literal(&mut self) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(20);
    let start = self.current_index;

    while let Some((pos, ch)) = self.consume() {
      if ch != '\'' { out_str.push(ch) }
      else { break; }
    } // while let some
    Ok((start, Token::AtomLiteral(out_str), self.current_index))
  }


  fn atom_literal(&mut self, first: char) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(10);
    out_str.push(first);
    let start = self.current_index;

    while let Some((pos, ch)) = self.consume() {
      if is_atom_char(ch) {
        out_str.push(ch);
      } else {
        self.un_consume();
        return Ok((start, Token::AtomLiteral(out_str), self.current_index));
      }
    };
    Ok((start, Token::AtomLiteral(out_str), self.current_index))
  }


  fn numeric_literal(&mut self, first: char) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(10);
    out_str.push(first);
    let start = self.current_index;

    while let Some((pos, ch)) = self.consume() {
      if is_atom_char(ch) { out_str.push(ch); }
      else { break; }
    };
    self.un_consume();
    let val = i64::from_str(out_str.as_str()).unwrap();
    Ok((start, Token::IntLiteral(val), self.current_index))
  }


  fn un_consume(&mut self) {
    if let Some(prev) = self.past.pop() {
      //println!("unconsume: {}", prev.1);
      self.upcoming.push(prev)
    }
  }


  /// Steps one forward with `self.input_iterator` and updates `next`, returning
  /// you the previous `next`
  fn consume(&mut self) -> Option<InputChar> {
    // Take if upcoming has some
    match self.upcoming.pop() {
      Some(curr) => {
        //println!("next: {} at {}", curr.1, curr.0);
        // If no more upcoming, fetch another one from `input`
        if self.upcoming.is_empty() {
          if let Some(nxt) = self.input.next() {
            self.upcoming.push(nxt)
          }
        }
        // Store currently fetched into `past`
        self.past.push(curr);
        Some(curr)
      },
      None => {
        //println!("next: None");
        None
      },
    }
  }


  fn escape_code(&mut self) -> Result<char, LexicalError> {
    match self.consume() {
      Some((_, ch)) => {
        match ch {
          '\'' => Ok('\''),
          '"' => Ok('"'),
          '\\' => Ok('\\'),
          '/' => Ok('/'),
          'n' => Ok('\n'),
          'r' => Ok('\r'),
          't' => Ok('\t'),
          'e' => Ok('\x1e'), // ESC code
          's' => Ok(' '),
          'v' => Ok('\x0b'), // vertical tab
          'f' => Ok('\x0c'), // form feed
          'b' => Ok('\x08'), // bell
          'd' => Ok('\x7f'), // ASCII delete
          other => self.err(LexicalError::UnexpectedEscapeCode(other)),
        }
      },
      None => self.err(LexicalError::UnexpectedEndOfFile),
    }
  }

  fn mk_tok(&self, t: Token) -> SpannedToken {
    (self.current_index, t, self.current_index)
  }

  fn is_digit_ahead(&self) -> bool {
    if let Some(upcoming) = self.look_ahead() {
      return upcoming.1.is_digit(10)
    };
    false
  }


  fn look_ahead(&self) -> Option<InputChar> {
    if self.upcoming.len() > 0 {
      let last = self.upcoming[self.upcoming.len() - 1];
      return Some(last)
    };
    None
  }
}

impl<'input> Iterator for Lexer<'input> {
  type Item = Result<SpannedToken, LexicalError>;

  fn next(&mut self) -> Option<Self::Item> {
    loop {
      match self.consume() {
        Some((i, ch)) => {
          match ch {
            '-' => return Some(Ok(self.mk_tok(Token::Minus))),
            ',' => return Some(Ok(self.mk_tok(Token::Comma))),
            '.' => return Some(Ok(self.mk_tok(Token::Dot))),

            '[' => return Some(Ok(self.mk_tok(Token::LSquareBracket))),
            '{' => return Some(Ok(self.mk_tok(Token::LCurlyBracket))),
            '(' => return Some(Ok(self.mk_tok(Token::LParen))),

            ']' => return Some(Ok(self.mk_tok(Token::RSquareBracket))),
            '}' => return Some(Ok(self.mk_tok(Token::RCurlyBracket))),
            ')' => return Some(Ok(self.mk_tok(Token::RParen))),

            '"' => return Some(self.string_literal()),
            '\'' => return Some(self.quoted_atom_literal()),

            '-' if self.is_digit_ahead() => return Some(self.numeric_literal('-')),
            ch if ch.is_digit(10) => return Some(self.numeric_literal(ch)),

            ch if is_atom_start(ch) => return Some(self.atom_literal(ch)),

            ch if is_whitespace(ch) => continue, // skip
            ch => return Some(self.err(LexicalError::Unexpected(ch))),
          };
        },
        None => return None,
      } // match next
    } // loop
  } // end next
}

#[inline]
fn is_atom_start(ch: char) -> bool { ch.is_ascii_lowercase() || ch == '_' }

#[inline]
fn is_atom_char(ch: char) -> bool { ch.is_ascii_alphanumeric() || ch == '_' }

#[inline]
fn is_whitespace(ch: char) -> bool { ch.is_whitespace() }
