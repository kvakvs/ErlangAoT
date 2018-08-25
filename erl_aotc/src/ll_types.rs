/// Module defines Erlang types as LLVM types.
/// A generic `term_t` contains:
///   * Mark word which defines value type
///
/// Bits in Mark word (element 0) define contents while the following words are
/// the content
///   * Atom
///   * Int32
///   * Int64
///   * Double

use llvm::*;
use llvm::core as ll;
use std::ffi::CString;


fn word_t() -> *mut LLVMType {
  i64_t()
}


fn i64_t() -> *mut LLVMType {
  unsafe { ll::LLVMInt64Type() }
}


fn i32_t() -> *mut LLVMType {
  unsafe { ll::LLVMInt32Type() }
}


#[allow(dead_code)]
fn i8_ptr_t() -> *mut LLVMType {
  unsafe {
    ll::LLVMPointerType(ll::LLVMInt8Type(), 0)
  }
}


fn term_fields() -> Vec<*mut LLVMType> {
  vec![
    word_t(),
  ]
}


/// Create a generic term-on-heap struct type which is vtable pointer + varying
/// fields. This is the default type for variabls and arguments.
pub fn create_term_type(context: *mut LLVMContext) -> *mut LLVMType {
  let mut t1_fields = term_fields();
  create_struct(context, "term_t", &mut t1_fields)
}


/// Create a 64-bit integer type (platform-independent)
pub fn create_i64_type(context: *mut LLVMContext) -> *mut LLVMType {
  let mut fields = term_fields();
  fields.push(i64_t());
  create_struct(context, "term_i64_t", &mut fields)
}


/// Create a 32-bit integer type (platform-independent)
pub fn create_i32_type(context: *mut LLVMContext) -> *mut LLVMType {
  let mut fields = term_fields();
  fields.push(i32_t());
  create_struct(context, "term_i32_t", &mut fields)
}


/// Create Atom type which is an index in Atom table
pub fn create_atom_type(context: *mut LLVMContext) -> *mut LLVMType {
  let mut fields = term_fields();
  fields.push(i64_t());
  create_struct(context, "term_atom_t", &mut fields)
}


fn create_struct(context: *mut LLVMContext, name: &str,
                 fields: &mut Vec<*mut LLVMType>) -> *mut LLVMType {
  let t1_name = CString::new(name).unwrap();

  unsafe {
    let t1 = ll::LLVMStructCreateNamed(context, t1_name.as_ptr());
    let fields_ptr = fields.as_mut_ptr();
    ll::LLVMStructSetBody(t1,
                          fields_ptr,
                          fields.len() as u32,
                          0);
    t1
  }
}
