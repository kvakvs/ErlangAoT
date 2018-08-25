pub mod codegen;
pub mod ll_types;

extern crate erl_shared;
extern crate llvm_sys as llvm;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
