#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::arch::asm;

// Syscall numbers
const ZSYS_EXIT: u64 = 3;
const ZSYS_OPEN: u64 = 4;
const ZSYS_READ: u64 = 5;
const ZSYS_WRITE: u64 = 6;
const ZSYS_CLOSE: u64 = 7;

const O_CREAT: u64 = 64;

#[repr(C)]
struct TarHeader {
    name: [u8; 100],
    mode: [u8; 8],
    uid: [u8; 8],
    gid: [u8; 8],
    size: [u8; 12],
    mtime: [u8; 12],
    checksum: [u8; 8],
    typeflag: u8,
    linkname: [u8; 100],
    magic: [u8; 6],
    version: [u8; 2],
    uname: [u8; 32],
    gname: [u8; 32],
    devmajor: [u8; 8],
    devminor: [u8; 8],
    prefix: [u8; 155],
    padding: [u8; 12],
}

fn octal_to_int(octal: &[u8]) -> usize {
    let mut res = 0;
    for &c in octal {
        if c == 0 || c == b' ' { break; }
        res = res * 8 + (c - b'0') as usize;
    }
    res
}

unsafe fn print_str(fd: u64, ptr: *const u8) {
    let mut len = 0;
    let mut temp = ptr;
    while *temp != 0 {
        len += 1;
        temp = temp.add(1);
    }
    sys_write(fd, ptr, len);
}

unsafe fn print_uint(fd: u64, mut val: u64) {
    if val == 0 {
        sys_write(fd, b"0\0".as_ptr(), 1);
        return;
    }
    let mut buf = [0u8; 20];
    let mut i = 19;
    while val > 0 {
        buf[i] = b'0' + (val % 10) as u8;
        val /= 10;
        i -= 1;
    }
    sys_write(fd, buf.as_ptr().add(i + 1), (19 - i) as u64);
}

unsafe fn is_str_equal(mut a: *const u8, mut b: *const u8) -> bool {
    while *a != 0 && *b != 0 {
        if *a != *b { return false; }
        a = a.add(1);
        b = b.add(1);
    }
    *a == *b
}

unsafe fn install_package(fd_tty: u64, pkg_name: *const u8) {
    let mut path = [0u8; 128];
    let prefix = b"/packages/";
    let suffix = b".tar";
    
    let mut i = 0;
    while i < prefix.len() { path[i] = prefix[i]; i += 1; }
    let mut j = 0;
    while *pkg_name.add(j) != 0 { path[i] = *pkg_name.add(j); i += 1; j += 1; }
    let mut k = 0;
    while k < suffix.len() { path[i] = suffix[k]; i += 1; k += 1; }
    path[i] = 0;

    let fd_pkg = sys_open(path.as_ptr(), 0);
    if fd_pkg < 0 {
        sys_write(fd_tty, b"Error: Package file not found: \0".as_ptr(), 31);
        print_str(fd_tty, path.as_ptr());
        sys_write(fd_tty, b"\n\0".as_ptr(), 1);
        return;
    }

    sys_write(fd_tty, b"Extracting package...\n\0".as_ptr(), 22);

    let mut header = core::mem::MaybeUninit::<TarHeader>::uninit();
    loop {
        let n = sys_read(fd_pkg as u64, header.as_mut_ptr() as *mut u8, 512);
        if n <= 0 { break; }
        
        let header_ref = &*header.as_ptr();
        if header_ref.name[0] == 0 { break; }

        let file_size = octal_to_int(&header_ref.size);
        
        let mut target_path = [0u8; 128];
        let bin_prefix = b"/bin/";
        let mut tp = 0;
        while tp < bin_prefix.len() { target_path[tp] = bin_prefix[tp]; tp += 1; }
        let mut hn = 0;
        while header_ref.name[hn] != 0 && tp < 127 { target_path[tp] = header_ref.name[hn]; tp += 1; hn += 1; }
        target_path[tp] = 0;

        sys_write(fd_tty, b"  -> \0".as_ptr(), 5);
        print_str(fd_tty, target_path.as_ptr());
        sys_write(fd_tty, b" (\0".as_ptr(), 2);
        print_uint(fd_tty, file_size as u64);
        sys_write(fd_tty, b" bytes)\n\0".as_ptr(), 8);

        let fd_out = sys_open(target_path.as_ptr(), O_CREAT);
        
        let blocks = (file_size + 511) / 512;
        let mut remaining = file_size;
        for _ in 0..blocks {
            let mut data = [0u8; 512];
            sys_read(fd_pkg as u64, data.as_mut_ptr(), 512);
            if fd_out >= 0 {
                let to_write = if remaining < 512 { remaining } else { 512 };
                sys_write(fd_out as u64, data.as_ptr(), to_write as u64);
                remaining -= to_write;
            }
        }
        if fd_out >= 0 {
            sys_close(fd_out as i32);
        }
    }

    sys_close(fd_pkg as i32);
    sys_write(fd_tty, b"Installation complete.\n\0".as_ptr(), 23);
}

#[no_mangle]
pub unsafe extern "C" fn _start() -> ! {
    let ustack_ptr: *const u64;
    asm!("mov {}, rsp", out(reg) ustack_ptr);

    let argc = *ustack_ptr;
    let argv = ustack_ptr.add(1);

    let fd = sys_open(b"/dev/tty\0".as_ptr(), 0);
    
    if fd >= 0 {
        if argc > 1 {
            let cmd_ptr = *argv.add(1) as *const u8;
            if is_str_equal(cmd_ptr, b"install\0".as_ptr()) {
                if argc > 2 {
                    let pkg_ptr = *argv.add(2) as *const u8;
                    install_package(fd as u64, pkg_ptr);
                } else {
                    sys_write(fd as u64, b"Usage: zpm install <package>\n\0".as_ptr(), 29);
                }
            } else {
                sys_write(fd as u64, b"Unknown command\n\0".as_ptr(), 16);
            }
        } else {
            sys_write(fd as u64, b"Zoho Package Manager v0.1.0\nUsage: zpm install <package>\n\0".as_ptr(), 55);
        }
    }

    sys_exit(0);
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// --- Syscall Bridge ---

unsafe fn sys_call1(num: u64, a1: u64) -> u64 {
    let ret: u64;
    asm!("syscall", in("rax") num, in("rdi") a1, out("rcx") _, out("r11") _, lateout("rax") ret);
    ret
}

unsafe fn sys_call2(num: u64, a1: u64, a2: u64) -> u64 {
    let ret: u64;
    asm!("syscall", in("rax") num, in("rdi") a1, in("rsi") a2, out("rcx") _, out("r11") _, lateout("rax") ret);
    ret
}

unsafe fn sys_call3(num: u64, a1: u64, a2: u64, a3: u64) -> u64 {
    let ret: u64;
    asm!("syscall", in("rax") num, in("rdi") a1, in("rsi") a2, in("rdx") a3, out("rcx") _, out("r11") _, lateout("rax") ret);
    ret
}

pub unsafe fn sys_open(path: *const u8, flags: u64) -> i64 {
    sys_call2(ZSYS_OPEN, path as u64, flags) as i64
}

pub unsafe fn sys_read(fd: u64, buf: *mut u8, count: u64) -> i64 {
    sys_call3(ZSYS_READ, fd, buf as u64, count) as i64
}

pub unsafe fn sys_write(fd: u64, buf: *const u8, count: u64) -> i64 {
    sys_call3(ZSYS_WRITE, fd, buf as u64, count) as i64
}

pub unsafe fn sys_close(fd: i32) {
    sys_call1(ZSYS_CLOSE, fd as u64);
}

pub unsafe fn sys_exit(status: i32) -> ! {
    sys_call1(ZSYS_EXIT, status as u64);
    loop {}
}
