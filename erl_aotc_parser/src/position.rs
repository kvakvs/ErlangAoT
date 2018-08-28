// TODO: Make this line:column, just line is helpful already
#[derive(Clone, Copy)]
pub struct Position {
  line: usize,
}

impl Position {
  pub fn new() -> Position {
    Position { line: 1 }
  }


  pub fn get_next(self, ch: char) -> Position {
    match ch {
      '\n' => Position { line: self.line + 1 },
      _ => self,
    }
  }
}
