use erlang_term::DottedTermParser;
use std::fs::File;
use std::io::Read;

pub fn compile(filename: &str) {
  println!("aotc: Reading file {}", filename);
  let mut file = File::open(filename).unwrap();
  let mut contents = String::new();
  file.read_to_string(&mut contents).unwrap();

  let out_term = DottedTermParser::new().parse(contents.as_str()).unwrap();
  println!("Parsed: {:?}", out_term)
}