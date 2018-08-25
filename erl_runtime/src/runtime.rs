use std::alloc;
use std::mem::size_of;

//use erl_shared::rt_types;


#[link(name = "erlrt_alloc")]
pub fn alloc(n: usize) -> *mut u8 {
  unsafe {
    let layo = alloc::Layout::from_size_align(
      n, size_of::<usize>()
    ).unwrap();
    alloc::alloc(layo)
  }
}
