//use erl_shared::fterm::FTerm;
use std::fs::File;
use std::io::Read;
use erl_aotc_parser::parse_nodot;


pub fn compile(filename: &str) {
  println!("aotc: Reading file {}", filename);
  let mut file = File::open(filename).unwrap();
  let mut contents = String::new();
  file.read_to_string(&mut contents).unwrap();

  let out_term = parse_nodot(contents.as_str());
  println!("Parsed: {:?}", out_term)
}
