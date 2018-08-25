use ll_types;

use llvm::*;
use llvm::core as ll;
use std::ffi::CString;


pub struct Codegen {
  pub context: *mut LLVMContext,
  pub builder: *mut LLVMBuilder,
  pub word_size: usize,

  pub term_type: *mut LLVMType,
  pub term_ptr_type: *mut LLVMType,
  pub atom_type: *mut LLVMType,
  pub i32_type: *mut LLVMType,
  pub i64_type: *mut LLVMType,
}


impl Codegen {
  pub fn new() -> Codegen {
    unsafe {
      let context = ll::LLVMContextCreate();
      let builder = ll::LLVMCreateBuilderInContext(context);
      let term_type = ll_types::create_term_type(context);
      Codegen {
        context,
        builder,
        term_type,
        term_ptr_type: ll::LLVMPointerType(term_type, 0),
        atom_type: ll_types::create_atom_type(context),
        i32_type: ll_types::create_i32_type(context),
        i64_type: ll_types::create_i64_type(context),
        word_size: 8
      }
    }
  }


  pub fn new_module(&mut self, name: &str) -> *mut LLVMModule {
    let n = CString::new(name).unwrap();
    let new_mod = unsafe { ll::LLVMModuleCreateWithName(n.as_ptr()) };
    new_mod
  }


  /// Create a new function in module.
  pub fn new_fun(&self,
                 module: *mut LLVMModule,
                 name: &str,
                 mut args: Vec<*mut LLVMType>) -> *mut LLVMValue {
    // Get the type signature for void nop(void);
    // Then create it in our module.
    unsafe {
      let args_ptr = args.as_mut_ptr();
      let ret_type = ll::LLVMFunctionType(
        self.term_ptr_type,
        args_ptr,
        args.len() as u32,
        0,
      );
      let f_name = CString::new(name).unwrap();
      let new_fun = ll::LLVMAddFunction(
        module,
        f_name.as_ptr(),
        ret_type
      );
      self.new_fun_block(new_fun, "fun_main");
      new_fun
    }
  }


  pub fn new_fun_block(&self,
                       fun: *mut LLVMValue,
                       label_name: &str) -> *const LLVMBasicBlock {
    let lname = CString::new(label_name).unwrap();
    unsafe {
      let bb = ll::LLVMAppendBasicBlockInContext(
        self.context, fun, lname.as_ptr());
      ll::LLVMPositionBuilderAtEnd(self.builder, bb);
      bb
    }
  }


  pub fn new_ret(&self, val: *mut LLVMValue) {
    unsafe {
      ll::LLVMBuildRet(self.builder, val);
    }
  }
}

