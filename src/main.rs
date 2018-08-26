//! Construct a function that does nothing in LLVM IR.

extern crate llvm_sys as llvm;

use llvm::core as ll;
use erl_aotc::codegen::Codegen;

extern crate erl_shared;
extern crate erl_aotc;
//extern crate erl_runtime;

use erl_aotc::aotc_main;


fn main() {
  aotc_main::compile("experiment/mochijson.kernel.term");

//  unsafe {
//    // Set up a context, module and builder in that context.
//    let mut cgen = Codegen::new();
//    let m = cgen.new_module("erl_test");
//
//    let f = cgen.new_fun(
//      m,
//      "erl_main",
//      vec! [cgen.term_ptr_type, cgen.term_ptr_type]
//    );
//
//    // Create a basic block in the function and set our builder to generate
//    // code in it.
//    cgen.new_ret(ll::LLVMGetParam(f, 0));
//
//    // Dump the module as IR to stdout.
//    ll::LLVMDumpModule(m);
//
//    // Clean up. Values created in the context mostly get cleaned up there.
//    ll::LLVMDisposeBuilder(cgen.builder);
//    ll::LLVMDisposeModule(m);
//    ll::LLVMContextDispose(cgen.context);
//  }
}
