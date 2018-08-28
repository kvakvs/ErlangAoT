use position::Position;
use std::str::CharIndices;
use token::Token;

#[derive(Debug, Copy, Clone)]
pub enum LexicalError {
  UnterminatedStringLiteral,
  UnexpectedEscapeCode(char),
  UnexpectedEndOfFile,
  Unexpected(char, usize),
}


pub type Spanned<Token, Loc, Error> = Result<(Loc, Token, Loc), Error>;

pub type SpannedToken = (usize, Token, usize);


pub struct Lexer<'input> {
  // Current line (position) in the input for error reporting
  current_pos: Position,
  // Input character index (not very useful for error reporting)
  current_index: usize,
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
      current_index: 0,
      next,
    }
  }


  fn string_literal(&mut self) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(32);

    let start = self.current_index;

    while let Some((pos, ch)) = self.step_forward() {
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

    while let Some((pos, ch)) = self.step_forward() {
      if ch != '\'' { out_str.push(ch) }
      else { break; }
    } // while let some
    Ok((start, Token::AtomLiteral(out_str), 0))
  }


  fn atom_literal(&mut self, first: char) -> Result<SpannedToken, LexicalError> {
    let mut out_str = String::new();
    out_str.reserve(10);
    out_str.push(first);
    let start = self.current_index;

    // Previous values for step-back by 1
    let mut prev_iter = self.input_iter.clone();
    let mut prev_next = self.next;

    while let Some((pos, ch)) = self.step_forward() {
      if is_atom_char(ch) {
        out_str.push(ch);
        prev_iter = self.input_iter.clone();
        prev_next = self.next.clone();
      }
      else {
        self.input_iter = prev_iter; // unread 1 back
        self.next = prev_next;
        break;
      }
    };
    Ok((start, Token::AtomLiteral(out_str), 0))
  }


  /// Steps one forward with `self.input_iterator` and updates `next`, returning
  /// you the previous `next`
  fn step_forward(&mut self) -> Option<(usize, char)> {
    match self.next {
      Some((index, ch)) => {
        self.current_index = index;
        self.next = self.input_iter.next();
        println!("next: {}", ch);
        Some((index, ch))
      },
      None => None,
    }
  }


  fn escape_code(&mut self) -> Result<char, LexicalError> {
    match self.step_forward() {
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

  fn mk_stoken(&self, t: Token) -> SpannedToken {
    (0, t, 0)
  }
}

impl<'input> Iterator for Lexer<'input> {
  type Item = Result<SpannedToken, LexicalError>;

  fn next(&mut self) -> Option<Self::Item> {
    loop {
      match self.step_forward() {
        Some((i, ch)) => {
          match ch {
            '-' => return Some(Ok(self.mk_stoken(Token::Minus))),
            ',' => return Some(Ok(self.mk_stoken(Token::Comma))),
            '.' => return Some(Ok(self.mk_stoken(Token::Dot))),

            '[' => return Some(Ok(self.mk_stoken(Token::LSquareBracket))),
            '{' => return Some(Ok(self.mk_stoken(Token::LCurlyBracket))),
            '(' => return Some(Ok(self.mk_stoken(Token::LParen))),

            ']' => return Some(Ok(self.mk_stoken(Token::RSquareBracket))),
            '}' => return Some(Ok(self.mk_stoken(Token::RCurlyBracket))),
            ')' => return Some(Ok(self.mk_stoken(Token::RParen))),

            '"' => return Some(self.string_literal()),
            '\'' => return Some(self.quoted_atom_literal()),

            ch if is_atom_start(ch) => return Some(self.atom_literal(ch)),

            ch if is_whitespace(ch) => continue, // skip
            ch => return Some(Err(LexicalError::Unexpected(ch, i))),
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
