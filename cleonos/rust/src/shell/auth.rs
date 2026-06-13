use super::*;

pub(crate) fn env_value(key: &[u8], out: &mut [u8]) -> bool {
    clear_buf(out);
    let envc = syscall(SYSCALL_PROC_ENVC, 0, 0, 0);
    let key_len = key.len();
    let mut i = 0u64;
    let mut item = [0u8; 128];
    while i < envc {
        clear_buf(&mut item);
        if syscall(SYSCALL_PROC_ENV, i, item.as_mut_ptr() as U64, item.len() as U64) != 0 {
            let item_len = c_len(&item);
            if item_len > key_len && item[..key_len] == *key && item[key_len] == b'=' {
                let _ = copy_bytes(out, &item[key_len + 1..item_len]);
                return true;
            }
        }
        i += 1;
    }
    false
}

pub(crate) fn apply_user_info(sh: &mut ShellState, info: &UserInfo) {
    let _ = copy_bytes(&mut sh.username, &info.name[..c_len(&info.name)]);
    let _ = copy_bytes(&mut sh.home, &info.home[..c_len(&info.home)]);
    sh.role = info.role;
    sh.disk_login_required = info.disk_login_required != 0;
    sh.logged_in = info.logged_in != 0;
    if info.home[0] == b'/' && syscall(SYSCALL_FS_STAT_TYPE, info.home.as_ptr() as U64, 0, 0) == 2 {
        let _ = copy_bytes(&mut sh.cwd, &info.home[..c_len(&info.home)]);
    } else {
        let _ = copy_bytes(&mut sh.cwd, b"/");
    }
}

pub(crate) fn login_if_needed(sh: &mut ShellState) -> bool {
    let mut info = UserInfo {
        uid: 0,
        role: 0,
        logged_in: 0,
        disk_login_required: 0,
        name: [0; NAME_MAX],
        home: [0; HOME_MAX],
    };
    if syscall(SYSCALL_USER_CURRENT, &mut info as *mut _ as U64, 0, 0) != 0 {
        sh.disk_login_required = info.disk_login_required != 0;
        if info.logged_in != 0 {
            apply_user_info(sh, &info);
            return true;
        }
    }
    if !sh.disk_login_required {
        let name = b"root\0";
        let password = b"\0";
        let mut login_info = UserInfo {
            uid: 0,
            role: 0,
            logged_in: 0,
            disk_login_required: 0,
            name: [0; NAME_MAX],
            home: [0; HOME_MAX],
        };
        let req = UserLoginReq {
            name_ptr: name.as_ptr() as U64,
            password_ptr: password.as_ptr() as U64,
            out_info_ptr: &mut login_info as *mut _ as U64,
        };
        let _ = syscall(SYSCALL_USER_LOGIN, &req as *const _ as U64, 0, 0);
        let _ = copy_bytes(&mut sh.username, b"root");
        let _ = copy_bytes(&mut sh.home, b"/");
        let _ = copy_bytes(&mut sh.cwd, b"/");
        sh.role = USER_ROLE_ADMIN;
        sh.logged_in = true;
        return true;
    }

    writeln(b"CLeonOS disk login\0");
    loop {
        let mut name = [0u8; NAME_MAX];
        let mut password = [0u8; 96];
        read_plain_line(b"login: ", &mut name, false);
        trim_ascii_in_place(&mut name);
        if c_len(&name) == 0 {
            continue;
        }
        read_plain_line(b"password: ", &mut password, true);
        let mut login_info = UserInfo {
            uid: 0,
            role: 0,
            logged_in: 0,
            disk_login_required: 0,
            name: [0; NAME_MAX],
            home: [0; HOME_MAX],
        };
        let req = UserLoginReq {
            name_ptr: name.as_ptr() as U64,
            password_ptr: password.as_ptr() as U64,
            out_info_ptr: &mut login_info as *mut _ as U64,
        };
        if syscall(SYSCALL_USER_LOGIN, &req as *const _ as U64, 0, 0) != 0 {
            apply_user_info(sh, &login_info);
            write_bytes(b"login: welcome ");
            write_cstr(&login_info.name);
            write_bytes(b"\n");
            return true;
        }
        writeln(b"login: invalid username or password\0");
    }
}
