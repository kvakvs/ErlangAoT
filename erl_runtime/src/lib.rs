// Create .a (windows .lib) static library
//#![crate_type="staticlib"]

extern crate erl_shared;

pub mod runtime;


#[cfg(test)]
mod tests {
  #[test]
  fn it_works() {
    assert_eq!(2 + 2, 4);
  }
}
