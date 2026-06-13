use super::*;

extern "C" {
    fn cleonos_syscall(id: U64, arg0: U64, arg1: U64, arg2: U64) -> U64;
}

pub(crate) fn syscall(id: U64, arg0: U64, arg1: U64, arg2: U64) -> U64 {
    unsafe { cleonos_syscall(id, arg0, arg1, arg2) }
}
