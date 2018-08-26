use std::alloc;
use std::mem::size_of;

//use erl_shared::rt_types;


#[no_mangle]
pub extern "C" fn erlrt_alloc(n: usize) -> *mut u8 {
  unsafe {
    let layo = alloc::Layout::from_size_align(
      n, size_of::<usize>()
    ).unwrap();
    alloc::alloc(layo)
  }
}
