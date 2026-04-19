from __future__ import annotations

import os
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .constants import (
    DEFAULT_MAX_EXEC_DEPTH,
    FD_INHERIT,
    FS_NAME_MAX,
    MAX_CSTR,
    MAX_IO_READ,
    O_APPEND,
    O_CREAT,
    O_RDONLY,
    O_RDWR,
    O_TRUNC,
    O_WRONLY,
    PAGE_SIZE,
    PROC_STATE_EXITED,
    PROC_STATE_PENDING,
    PROC_STATE_RUNNING,
    PROC_STATE_STOPPED,
    SIGCONT,
    SIGKILL,
    SIGSTOP,
    SIGTERM,
    SYS_AUDIO_AVAILABLE,
    SYS_AUDIO_PLAY_TONE,
    SYS_AUDIO_STOP,
    SYS_CONTEXT_SWITCHES,
    SYS_CUR_TASK,
    SYS_DL_CLOSE,
    SYS_DL_OPEN,
    SYS_DL_SYM,
    SYS_EXEC_PATH,
    SYS_EXEC_PATHV,
    SYS_EXEC_PATHV_IO,
    SYS_EXEC_REQUESTS,
    SYS_EXEC_SUCCESS,
    SYS_EXIT,
    SYS_FD_CLOSE,
    SYS_FD_DUP,
    SYS_FD_OPEN,
    SYS_FD_READ,
    SYS_FD_WRITE,
    SYS_FS_APPEND,
    SYS_FS_CHILD_COUNT,
    SYS_FS_GET_CHILD_NAME,
    SYS_FS_MKDIR,
    SYS_FS_NODE_COUNT,
    SYS_FS_READ,
    SYS_FS_REMOVE,
    SYS_FS_STAT_SIZE,
    SYS_FS_STAT_TYPE,
    SYS_FS_WRITE,
    SYS_GETPID,
    SYS_KBD_BUFFERED,
    SYS_KBD_DROPPED,
    SYS_KBD_GET_CHAR,
    SYS_KBD_HOTKEY_SWITCHES,
    SYS_KBD_POPPED,
    SYS_KBD_PUSHED,
    SYS_KELF_COUNT,
    SYS_KELF_RUNS,
    SYS_LOG_JOURNAL_COUNT,
    SYS_LOG_JOURNAL_READ,
    SYS_LOG_WRITE,
    SYS_PROC_ARGC,
    SYS_PROC_ARGV,
    SYS_PROC_COUNT,
    SYS_PROC_ENVC,
    SYS_PROC_ENV,
    SYS_PROC_FAULT_ERROR,
    SYS_PROC_FAULT_RIP,
    SYS_PROC_FAULT_VECTOR,
    SYS_PROC_KILL,
    SYS_PROC_LAST_SIGNAL,
    SYS_PROC_PID_AT,
    SYS_PROC_SNAPSHOT,
    SYS_RESTART,
    SYS_SERVICE_COUNT,
    SYS_SERVICE_READY_COUNT,
    SYS_SHUTDOWN,
    SYS_SLEEP_TICKS,
    SYS_SPAWN_PATH,
    SYS_SPAWN_PATHV,
    SYS_STATS_ID_COUNT,
    SYS_STATS_RECENT_ID,
    SYS_STATS_RECENT_WINDOW,
    SYS_STATS_TOTAL,
    SYS_TASK_COUNT,
    SYS_TIMER_TICKS,
    SYS_TTY_ACTIVE,
    SYS_TTY_COUNT,
    SYS_TTY_SWITCH,
    SYS_TTY_WRITE,
    SYS_TTY_WRITE_CHAR,
    SYS_USER_EXEC_REQUESTED,
    SYS_USER_LAUNCH_FAIL,
    SYS_USER_LAUNCH_OK,
    SYS_USER_LAUNCH_TRIES,
    SYS_USER_SHELL_READY,
    SYS_WAITPID,
    SYS_YIELD,
    page_ceil,
    page_floor,
    u64,
    u64_neg1,
)
from .input_pump import InputPump
from .platform import (
    Uc,
    UcError,
    UC_ARCH_X86,
    UC_ERR_FETCH_PROT,
    UC_ERR_FETCH_UNMAPPED,
    UC_ERR_INSN_INVALID,
    UC_ERR_READ_PROT,
    UC_ERR_READ_UNMAPPED,
    UC_ERR_WRITE_PROT,
    UC_ERR_WRITE_UNMAPPED,
    UC_HOOK_CODE,
    UC_HOOK_INTR,
    UC_MODE_64,
    UC_PROT_ALL,
    UC_PROT_EXEC,
    UC_PROT_READ,
    UC_PROT_WRITE,
    UC_X86_REG_RAX,
    UC_X86_REG_RBP,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDX,
    UC_X86_REG_RSP,
    UC_X86_REG_RIP,
)
from .state import SharedKernelState


@dataclass
class ELFSegment:
    vaddr: int
    memsz: int
    flags: int
    data: bytes


@dataclass
class ELFImage:
    entry: int
    segments: List[ELFSegment]


@dataclass
class FDEntry:
    kind: str
    flags: int
    offset: int = 0
    path: str = ""
    tty_index: int = 0


EXEC_PATH_MAX = 192
EXEC_ARG_LINE_MAX = 256
EXEC_ENV_LINE_MAX = 512
EXEC_MAX_ARGS = 24
EXEC_MAX_ENVS = 24
EXEC_ITEM_MAX = 128
EXEC_STATUS_SIGNAL_FLAG = 1 << 63
PROC_PATH_MAX = 192
FD_MAX = 64


class CLeonOSWineNative:
    def __init__(
        self,
        elf_path: Path,
        rootfs: Path,
        guest_path_hint: str,
        *,
        state: Optional[SharedKernelState] = None,
        depth: int = 0,
        max_exec_depth: int = DEFAULT_MAX_EXEC_DEPTH,
        no_kbd: bool = False,
        verbose: bool = False,
        top_level: bool = True,
        pid: int = 0,
        ppid: int = 0,
        argv_items: Optional[List[str]] = None,
        env_items: Optional[List[str]] = None,
        inherited_fds: Optional[Dict[int, FDEntry]] = None,
    ) -> None:
        self.elf_path = elf_path
        self.rootfs = rootfs
        self.guest_path_hint = guest_path_hint
        self.state = state if state is not None else SharedKernelState()
        self.depth = depth
        self.max_exec_depth = max_exec_depth
        self.no_kbd = no_kbd
        self.verbose = verbose
        self.top_level = top_level
        self.pid = int(pid)
        self.ppid = int(ppid)
        self.argv_items = list(argv_items) if argv_items is not None else []
        self.env_items = list(env_items) if env_items is not None else []
        self._exit_requested = False
        self._exit_status = 0

        self.image = self._parse_elf(self.elf_path)
        self.exit_code: Optional[int] = None
        self._input_pump: Optional[InputPump] = None

        self._stack_base = 0x00007FFF00000000
        self._stack_size = 0x0000000000020000
        self._ret_sentinel = 0x00007FFF10000000
        self._mapped_ranges: List[Tuple[int, int]] = []
        self._tty_index = int(self.state.tty_active)
        self._fds: Dict[int, FDEntry] = {}
        self._fd_inherited = inherited_fds if inherited_fds is not None else {}

        default_path = self._normalize_guest_path(self.guest_path_hint or f"/{self.elf_path.name}")
        self.argv_items, self.env_items = self._prepare_exec_items(default_path, self.argv_items, self.env_items)
        self._init_default_fds()

    @staticmethod
    def _clone_fd_entry(entry: FDEntry) -> FDEntry:
        return FDEntry(kind=entry.kind, flags=int(entry.flags), offset=int(entry.offset), path=str(entry.path), tty_index=int(entry.tty_index))

    @staticmethod
    def _fd_access_mode(flags: int) -> int:
        return int(flags) & 0x3

    @classmethod
    def _fd_access_mode_valid(cls, flags: int) -> bool:
        mode = cls._fd_access_mode(flags)
        return mode in (O_RDONLY, O_WRONLY, O_RDWR)

    @classmethod
    def _fd_can_read(cls, flags: int) -> bool:
        mode = cls._fd_access_mode(flags)
        return mode in (O_RDONLY, O_RDWR)

    @classmethod
    def _fd_can_write(cls, flags: int) -> bool:
        mode = cls._fd_access_mode(flags)
        return mode in (O_WRONLY, O_RDWR)

    def _init_default_fds(self) -> None:
        self._fds = {
            0: FDEntry(kind="tty", flags=O_RDONLY, offset=0, tty_index=self._tty_index),
            1: FDEntry(kind="tty", flags=O_WRONLY, offset=0, tty_index=self._tty_index),
            2: FDEntry(kind="tty", flags=O_WRONLY, offset=0, tty_index=self._tty_index),
        }

        for target in (0, 1, 2):
            inherited = self._fd_inherited.get(target)
            if inherited is not None:
                self._fds[target] = self._clone_fd_entry(inherited)

        for target in (0, 1, 2):
            entry = self._fds.get(target)
            if entry is not None and entry.kind == "tty":
                self._tty_index = int(entry.tty_index)
                break

    def _fd_lookup(self, fd: int) -> Optional[FDEntry]:
        if fd < 0 or fd >= FD_MAX:
            return None
        return self._fds.get(int(fd))

    def _fd_find_free(self) -> int:
        for fd in range(FD_MAX):
            if fd not in self._fds:
                return fd
        return -1

    def _stdio_entry_for_child(self, target_fd: int, override_fd: int, require_read: bool, require_write: bool) -> Optional[FDEntry]:
        if override_fd == FD_INHERIT:
            src = self._fd_lookup(target_fd)
        else:
            src = self._fd_lookup(override_fd)

        if src is None:
            return None

        if require_read and not self._fd_can_read(src.flags):
            return None

        if require_write and not self._fd_can_write(src.flags):
            return None

        return self._clone_fd_entry(src)

    def _build_child_stdio_map(self, stdin_fd: int, stdout_fd: int, stderr_fd: int) -> Optional[Dict[int, FDEntry]]:
        child_map: Dict[int, FDEntry] = {}

        in_entry = self._stdio_entry_for_child(0, stdin_fd, require_read=True, require_write=False)
        out_entry = self._stdio_entry_for_child(1, stdout_fd, require_read=False, require_write=True)
        err_entry = self._stdio_entry_for_child(2, stderr_fd, require_read=False, require_write=True)

        if in_entry is None or out_entry is None or err_entry is None:
            return None

        child_map[0] = in_entry
        child_map[1] = out_entry
        child_map[2] = err_entry
        return child_map

    def run(self) -> Optional[int]:
        if self.pid == 0:
            self.pid = self.state.alloc_pid(self.ppid)

        prev_pid = self.state.get_current_pid()
        self.state.set_current_pid(self.pid)
        self.state.set_proc_cmdline(self.pid, self.argv_items, self.env_items)
        self.state.set_proc_fault(self.pid, 0, 0, 0, 0)
        self.state.set_proc_running(self.pid, self.argv_items[0] if self.argv_items else self.guest_path_hint, self._tty_index)

        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self._install_hooks(uc)
        self._load_segments(uc)
        self._prepare_stack_and_return(uc)

        if self.top_level and not self.no_kbd:
            self._input_pump = InputPump(self.state)
            self._input_pump.start()

        interrupted = False
        runtime_fault_status: Optional[int] = None

        try:
            uc.emu_start(self.image.entry, 0)
        except KeyboardInterrupt:
            interrupted = True
            if self.top_level:
                print("\n[WINE] interrupted by user", file=sys.stderr)
        except UcError as exc:
            runtime_fault_status = self._status_from_uc_error(uc, exc)
            if self.verbose or self.top_level:
                print(f"[WINE][ERROR] runtime crashed: {exc}", file=sys.stderr)
        finally:
            if self.top_level and self._input_pump is not None:
                self._input_pump.stop()

        if interrupted:
            self.state.mark_exited(self.pid, u64_neg1())
            self.state.set_current_pid(prev_pid)
            return None

        if runtime_fault_status is not None:
            self.exit_code = runtime_fault_status

        if self.exit_code is None:
            self.exit_code = self._reg_read(uc, UC_X86_REG_RAX)

        if self._exit_requested:
            self.exit_code = self._exit_status

        self.exit_code = u64(self.exit_code)
        self.state.mark_exited(self.pid, self.exit_code)
        self.state.set_current_pid(prev_pid)
        return self.exit_code

    def _install_hooks(self, uc: Uc) -> None:
        uc.hook_add(UC_HOOK_INTR, self._hook_intr)
        uc.hook_add(UC_HOOK_CODE, self._hook_code, begin=self._ret_sentinel, end=self._ret_sentinel)

    def _hook_code(self, uc: Uc, address: int, size: int, _user_data) -> None:
        _ = size
        if address == self._ret_sentinel:
            self.exit_code = self._reg_read(uc, UC_X86_REG_RAX)
            uc.emu_stop()

    def _hook_intr(self, uc: Uc, intno: int, _user_data) -> None:
        if intno != 0x80:
            raise UcError(1)

        syscall_id = self._reg_read(uc, UC_X86_REG_RAX)
        arg0 = self._reg_read(uc, UC_X86_REG_RBX)
        arg1 = self._reg_read(uc, UC_X86_REG_RCX)
        arg2 = self._reg_read(uc, UC_X86_REG_RDX)

        self.state.record_syscall(syscall_id)
        self.state.context_switches = u64(self.state.context_switches + 1)
        ret = self._dispatch_syscall(uc, syscall_id, arg0, arg1, arg2)
        self._reg_write(uc, UC_X86_REG_RAX, u64(ret))

    def _dispatch_syscall(self, uc: Uc, sid: int, arg0: int, arg1: int, arg2: int) -> int:
        if sid == SYS_LOG_WRITE:
            data = self._read_guest_bytes(uc, arg0, arg1)
            text = data.decode("utf-8", errors="replace")
            self._host_write(text)
            self.state.log_journal_push(text)
            return len(data)
        if sid == SYS_TIMER_TICKS:
            return self.state.timer_ticks()
        if sid == SYS_TASK_COUNT:
            return self.state.task_count
        if sid == SYS_CUR_TASK:
            return self.state.current_task
        if sid == SYS_SERVICE_COUNT:
            return self.state.service_count
        if sid == SYS_SERVICE_READY_COUNT:
            return self.state.service_ready
        if sid == SYS_CONTEXT_SWITCHES:
            return self.state.context_switches
        if sid == SYS_KELF_COUNT:
            return self.state.kelf_count
        if sid == SYS_KELF_RUNS:
            return self.state.kelf_runs
        if sid == SYS_FS_NODE_COUNT:
            return self._fs_node_count()
        if sid == SYS_FS_CHILD_COUNT:
            return self._fs_child_count(uc, arg0)
        if sid == SYS_FS_GET_CHILD_NAME:
            return self._fs_get_child_name(uc, arg0, arg1, arg2)
        if sid == SYS_FS_READ:
            return self._fs_read(uc, arg0, arg1, arg2)
        if sid == SYS_EXEC_PATH:
            return self._exec_path(uc, arg0)
        if sid == SYS_EXEC_PATHV:
            return self._exec_pathv(uc, arg0, arg1, arg2)
        if sid == SYS_EXEC_PATHV_IO:
            return self._exec_pathv_io(uc, arg0, arg1, arg2)
        if sid == SYS_SPAWN_PATH:
            return self._spawn_path(uc, arg0)
        if sid == SYS_SPAWN_PATHV:
            return self._spawn_pathv(uc, arg0, arg1, arg2)
        if sid == SYS_WAITPID:
            return self._wait_pid(uc, arg0, arg1)
        if sid == SYS_GETPID:
            return self.state.get_current_pid()
        if sid == SYS_PROC_ARGC:
            return self._proc_argc()
        if sid == SYS_PROC_ARGV:
            return self._proc_argv(uc, arg0, arg1, arg2)
        if sid == SYS_PROC_ENVC:
            return self._proc_envc()
        if sid == SYS_PROC_ENV:
            return self._proc_env(uc, arg0, arg1, arg2)
        if sid == SYS_PROC_LAST_SIGNAL:
            return self._proc_last_signal()
        if sid == SYS_PROC_FAULT_VECTOR:
            return self._proc_fault_vector()
        if sid == SYS_PROC_FAULT_ERROR:
            return self._proc_fault_error()
        if sid == SYS_PROC_FAULT_RIP:
            return self._proc_fault_rip()
        if sid == SYS_PROC_COUNT:
            return self._proc_count()
        if sid == SYS_PROC_PID_AT:
            return self._proc_pid_at(uc, arg0, arg1)
        if sid == SYS_PROC_SNAPSHOT:
            return self._proc_snapshot(uc, arg0, arg1, arg2)
        if sid == SYS_PROC_KILL:
            return self._proc_kill(uc, arg0, arg1)
        if sid == SYS_EXIT:
            return self._request_exit(uc, arg0)
        if sid == SYS_SLEEP_TICKS:
            return self._sleep_ticks(arg0)
        if sid == SYS_YIELD:
            return self._yield_once()
        if sid == SYS_AUDIO_AVAILABLE:
            return 0
        if sid == SYS_AUDIO_PLAY_TONE:
            return 0
        if sid == SYS_AUDIO_STOP:
            return 1
        if sid == SYS_SHUTDOWN:
            self._host_write("\n[WINE] shutdown requested by guest\n")
            self._exit_requested = True
            self._exit_status = 0
            return 1
        if sid == SYS_RESTART:
            self._host_write("\n[WINE] restart requested by guest\n")
            self._exit_requested = True
            self._exit_status = 0
            return 1
        if sid == SYS_EXEC_REQUESTS:
            return self.state.exec_requests
        if sid == SYS_EXEC_SUCCESS:
            return self.state.exec_success
        if sid == SYS_USER_SHELL_READY:
            return self.state.user_shell_ready
        if sid == SYS_USER_EXEC_REQUESTED:
            return self.state.user_exec_requested
        if sid == SYS_USER_LAUNCH_TRIES:
            return self.state.user_launch_tries
        if sid == SYS_USER_LAUNCH_OK:
            return self.state.user_launch_ok
        if sid == SYS_USER_LAUNCH_FAIL:
            return self.state.user_launch_fail
        if sid == SYS_TTY_COUNT:
            return self.state.tty_count
        if sid == SYS_TTY_ACTIVE:
            return self.state.tty_active
        if sid == SYS_TTY_SWITCH:
            if arg0 >= self.state.tty_count:
                return u64_neg1()
            self.state.tty_active = int(arg0)
            return self.state.tty_active
        if sid == SYS_TTY_WRITE:
            data = self._read_guest_bytes(uc, arg0, arg1)
            self._host_write(data.decode("utf-8", errors="replace"))
            return len(data)
        if sid == SYS_TTY_WRITE_CHAR:
            ch = chr(arg0 & 0xFF)
            if ch in ("\b", "\x7f"):
                self._host_write("\b \b")
            else:
                self._host_write(ch)
            return 1
        if sid == SYS_KBD_GET_CHAR:
            key = self.state.pop_key()
            return u64_neg1() if key is None else key
        if sid == SYS_FS_STAT_TYPE:
            return self._fs_stat_type(uc, arg0)
        if sid == SYS_FS_STAT_SIZE:
            return self._fs_stat_size(uc, arg0)
        if sid == SYS_FS_MKDIR:
            return self._fs_mkdir(uc, arg0)
        if sid == SYS_FS_WRITE:
            return self._fs_write(uc, arg0, arg1, arg2)
        if sid == SYS_FS_APPEND:
            return self._fs_append(uc, arg0, arg1, arg2)
        if sid == SYS_FS_REMOVE:
            return self._fs_remove(uc, arg0)
        if sid == SYS_LOG_JOURNAL_COUNT:
            return self.state.log_journal_count()
        if sid == SYS_LOG_JOURNAL_READ:
            return self._log_journal_read(uc, arg0, arg1, arg2)
        if sid == SYS_KBD_BUFFERED:
            return self.state.buffered_count()
        if sid == SYS_KBD_PUSHED:
            return self.state.kbd_push_count
        if sid == SYS_KBD_POPPED:
            return self.state.kbd_pop_count
        if sid == SYS_KBD_DROPPED:
            return self.state.kbd_drop_count
        if sid == SYS_KBD_HOTKEY_SWITCHES:
            return self.state.kbd_hotkey_switches
        if sid == SYS_STATS_TOTAL:
            return self.state.stats_total()
        if sid == SYS_STATS_ID_COUNT:
            return self.state.stats_id_count(arg0)
        if sid == SYS_STATS_RECENT_WINDOW:
            return self.state.stats_recent_window()
        if sid == SYS_STATS_RECENT_ID:
            return self.state.stats_recent_id_count(arg0)
        if sid == SYS_FD_OPEN:
            return self._fd_open(uc, arg0, arg1, arg2)
        if sid == SYS_FD_READ:
            return self._fd_read(uc, arg0, arg1, arg2)
        if sid == SYS_FD_WRITE:
            return self._fd_write(uc, arg0, arg1, arg2)
        if sid == SYS_FD_CLOSE:
            return self._fd_close(arg0)
        if sid == SYS_FD_DUP:
            return self._fd_dup(arg0)
        if sid == SYS_DL_OPEN:
            return u64_neg1()
        if sid == SYS_DL_CLOSE:
            return 0
        if sid == SYS_DL_SYM:
            return 0

        return u64_neg1()

    def _host_write(self, text: str) -> None:
        if not text:
            return
        sys.stdout.write(text)
        sys.stdout.flush()

    def _host_write_bytes(self, data: bytes) -> None:
        if not data:
            return
        if hasattr(sys.stdout, "buffer"):
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
            return
        self._host_write(data.decode("utf-8", errors="replace"))

    def _load_segments(self, uc: Uc) -> None:
        for seg in self.image.segments:
            start = page_floor(seg.vaddr)
            end = page_ceil(seg.vaddr + seg.memsz)
            self._map_region(uc, start, end - start, UC_PROT_ALL)

        for seg in self.image.segments:
            if seg.data:
                self._mem_write(uc, seg.vaddr, seg.data)

        for seg in self.image.segments:
            start = page_floor(seg.vaddr)
            end = page_ceil(seg.vaddr + seg.memsz)
            size = end - start
            perms = 0
            if seg.flags & 0x4:
                perms |= UC_PROT_READ
            if seg.flags & 0x2:
                perms |= UC_PROT_WRITE
            if seg.flags & 0x1:
                perms |= UC_PROT_EXEC
            if perms == 0:
                perms = UC_PROT_READ
            try:
                uc.mem_protect(start, size, perms)
            except Exception:
                pass

    def _prepare_stack_and_return(self, uc: Uc) -> None:
        self._map_region(uc, self._stack_base, self._stack_size, UC_PROT_READ | UC_PROT_WRITE)
        self._map_region(uc, self._ret_sentinel, PAGE_SIZE, UC_PROT_READ | UC_PROT_EXEC)
        self._mem_write(uc, self._ret_sentinel, b"\x90")

        rsp = self._stack_base + self._stack_size - 8
        self._mem_write(uc, rsp, struct.pack("<Q", self._ret_sentinel))

        self._reg_write(uc, UC_X86_REG_RSP, rsp)
        self._reg_write(uc, UC_X86_REG_RBP, rsp)

    def _map_region(self, uc: Uc, addr: int, size: int, perms: int) -> None:
        if size <= 0:
            return
        start = page_floor(addr)
        end = page_ceil(addr + size)

        if self._is_range_mapped(start, end):
            return

        uc.mem_map(start, end - start, perms)
        self._mapped_ranges.append((start, end))

    def _is_range_mapped(self, start: int, end: int) -> bool:
        for ms, me in self._mapped_ranges:
            if start >= ms and end <= me:
                return True
        return False

    @staticmethod
    def _reg_read(uc: Uc, reg: int) -> int:
        return int(uc.reg_read(reg))

    @staticmethod
    def _reg_write(uc: Uc, reg: int, value: int) -> None:
        uc.reg_write(reg, u64(value))

    @staticmethod
    def _mem_write(uc: Uc, addr: int, data: bytes) -> None:
        if addr == 0 or not data:
            return
        uc.mem_write(addr, data)

    def _read_guest_cstring(self, uc: Uc, addr: int, max_len: int = MAX_CSTR) -> str:
        if addr == 0:
            return ""

        out = bytearray()
        for i in range(max_len):
            try:
                ch = uc.mem_read(addr + i, 1)
            except UcError:
                break
            if not ch or ch[0] == 0:
                break
            out.append(ch[0])
        return out.decode("utf-8", errors="replace")

    def _read_guest_bytes(self, uc: Uc, addr: int, size: int) -> bytes:
        if addr == 0 or size == 0:
            return b""
        safe_size = int(min(size, MAX_IO_READ))
        try:
            return bytes(uc.mem_read(addr, safe_size))
        except UcError:
            return b""

    def _read_guest_bytes_exact(self, uc: Uc, addr: int, size: int) -> Optional[bytes]:
        if size < 0 or addr == 0:
            return None
        if size == 0:
            return b""

        out = bytearray()
        cursor = int(addr)
        left = int(size)

        while left > 0:
            chunk = min(left, MAX_IO_READ)
            try:
                data = uc.mem_read(cursor, chunk)
            except UcError:
                return None
            if len(data) != chunk:
                return None
            out.extend(data)
            cursor += chunk
            left -= chunk

        return bytes(out)

    def _write_guest_bytes(self, uc: Uc, addr: int, data: bytes) -> bool:
        if addr == 0:
            return False
        try:
            uc.mem_write(addr, data)
            return True
        except UcError:
            return False

    @staticmethod
    def _parse_elf(path: Path) -> ELFImage:
        data = path.read_bytes()
        if len(data) < 64:
            raise RuntimeError(f"ELF too small: {path}")
        if data[0:4] != b"\x7fELF":
            raise RuntimeError(f"invalid ELF magic: {path}")
        if data[4] != 2 or data[5] != 1:
            raise RuntimeError(f"unsupported ELF class/endianness: {path}")

        entry = struct.unpack_from("<Q", data, 0x18)[0]
        phoff = struct.unpack_from("<Q", data, 0x20)[0]
        phentsize = struct.unpack_from("<H", data, 0x36)[0]
        phnum = struct.unpack_from("<H", data, 0x38)[0]

        if entry == 0:
            raise RuntimeError(f"ELF entry is 0: {path}")
        if phentsize == 0 or phnum == 0:
            raise RuntimeError(f"ELF has no program headers: {path}")

        segments: List[ELFSegment] = []
        for i in range(phnum):
            off = phoff + i * phentsize
            if off + 56 > len(data):
                break

            p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from(
                "<IIQQQQQQ", data, off
            )

            if p_type != 1 or p_memsz == 0:
                continue

            fs = int(p_filesz)
            fo = int(p_offset)
            if fs > 0:
                if fo >= len(data):
                    seg_data = b""
                else:
                    seg_data = data[fo : min(len(data), fo + fs)]
            else:
                seg_data = b""

            segments.append(ELFSegment(vaddr=int(p_vaddr), memsz=int(p_memsz), flags=int(p_flags), data=seg_data))

        if not segments:
            raise RuntimeError(f"ELF has no PT_LOAD segments: {path}")

        return ELFImage(entry=int(entry), segments=segments)

    def _fs_node_count(self) -> int:
        count = 1
        for _root, dirs, files in os.walk(self.rootfs):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            files = [f for f in files if not f.startswith(".")]
            count += len(dirs) + len(files)
        return count

    def _fs_child_count(self, uc: Uc, dir_ptr: int) -> int:
        path = self._read_guest_cstring(uc, dir_ptr)
        host_dir = self._guest_to_host(path, must_exist=True)
        if host_dir is None or not host_dir.is_dir():
            return u64_neg1()
        return len(self._list_children(host_dir))

    def _fs_get_child_name(self, uc: Uc, dir_ptr: int, index: int, out_ptr: int) -> int:
        if out_ptr == 0:
            return 0
        path = self._read_guest_cstring(uc, dir_ptr)
        host_dir = self._guest_to_host(path, must_exist=True)
        if host_dir is None or not host_dir.is_dir():
            return 0

        children = self._list_children(host_dir)
        if index >= len(children):
            return 0

        name = children[int(index)]
        encoded = name.encode("utf-8", errors="replace")
        if len(encoded) >= FS_NAME_MAX:
            encoded = encoded[: FS_NAME_MAX - 1]

        return 1 if self._write_guest_bytes(uc, out_ptr, encoded + b"\x00") else 0

    def _fs_read(self, uc: Uc, path_ptr: int, out_ptr: int, buf_size: int) -> int:
        if out_ptr == 0 or buf_size == 0:
            return 0

        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None or not host_path.is_file():
            return 0

        read_size = int(min(buf_size, MAX_IO_READ))
        try:
            data = host_path.read_bytes()[:read_size]
        except Exception:
            return 0

        if not data:
            return 0
        return len(data) if self._write_guest_bytes(uc, out_ptr, data) else 0

    def _fs_stat_type(self, uc: Uc, path_ptr: int) -> int:
        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return u64_neg1()
        if host_path.is_dir():
            return 2
        if host_path.is_file():
            return 1
        return u64_neg1()

    def _fs_stat_size(self, uc: Uc, path_ptr: int) -> int:
        path = self._read_guest_cstring(uc, path_ptr)
        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return u64_neg1()
        if host_path.is_dir():
            return 0
        if host_path.is_file():
            try:
                return host_path.stat().st_size
            except Exception:
                return u64_neg1()
        return u64_neg1()

    @staticmethod
    def _guest_path_is_under_temp(path: str) -> bool:
        return path == "/temp" or path.startswith("/temp/")

    def _fs_mkdir(self, uc: Uc, path_ptr: int) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))
        if not self._guest_path_is_under_temp(path):
            return 0

        host_path = self._guest_to_host(path, must_exist=False)
        if host_path is None:
            return 0

        if host_path.exists() and host_path.is_file():
            return 0

        try:
            host_path.mkdir(parents=True, exist_ok=True)
            return 1
        except Exception:
            return 0

    def _fs_write_common(self, uc: Uc, path_ptr: int, data_ptr: int, size: int, append_mode: bool) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))

        if not self._guest_path_is_under_temp(path) or path == "/temp":
            return 0

        if size < 0 or size > self.state.fs_write_max:
            return 0

        host_path = self._guest_to_host(path, must_exist=False)
        if host_path is None:
            return 0

        if host_path.exists() and host_path.is_dir():
            return 0

        data = b""
        if size > 0:
            if data_ptr == 0:
                return 0
            data = self._read_guest_bytes(uc, data_ptr, size)
            if len(data) != int(size):
                return 0

        try:
            host_path.parent.mkdir(parents=True, exist_ok=True)
            mode = "ab" if append_mode else "wb"
            with host_path.open(mode) as fh:
                if data:
                    fh.write(data)
            return 1
        except Exception:
            return 0

    def _fs_write(self, uc: Uc, path_ptr: int, data_ptr: int, size: int) -> int:
        return self._fs_write_common(uc, path_ptr, data_ptr, size, append_mode=False)

    def _fs_append(self, uc: Uc, path_ptr: int, data_ptr: int, size: int) -> int:
        return self._fs_write_common(uc, path_ptr, data_ptr, size, append_mode=True)

    def _fs_remove(self, uc: Uc, path_ptr: int) -> int:
        path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr))

        if not self._guest_path_is_under_temp(path) or path == "/temp":
            return 0

        host_path = self._guest_to_host(path, must_exist=True)
        if host_path is None:
            return 0

        try:
            if host_path.is_dir():
                if any(host_path.iterdir()):
                    return 0
                host_path.rmdir()
            else:
                host_path.unlink()
            return 1
        except Exception:
            return 0

    def _log_journal_read(self, uc: Uc, index_from_oldest: int, out_ptr: int, out_size: int) -> int:
        if out_ptr == 0 or out_size == 0:
            return 0

        line = self.state.log_journal_read(int(index_from_oldest))
        if line is None:
            return 0

        encoded = line.encode("utf-8", errors="replace")
        max_payload = int(out_size) - 1
        if max_payload < 0:
            return 0

        if len(encoded) > max_payload:
            encoded = encoded[:max_payload]

        return 1 if self._write_guest_bytes(uc, out_ptr, encoded + b"\x00") else 0

    def _exec_path(self, uc: Uc, path_ptr: int) -> int:
        return self._spawn_path_common(
            uc,
            path_ptr,
            0,
            0,
            return_pid=False,
            env_line_override=None,
            stdin_fd=FD_INHERIT,
            stdout_fd=FD_INHERIT,
            stderr_fd=FD_INHERIT,
        )

    def _exec_pathv(self, uc: Uc, path_ptr: int, argv_ptr: int, env_ptr: int) -> int:
        return self._spawn_path_common(
            uc,
            path_ptr,
            argv_ptr,
            env_ptr,
            return_pid=False,
            env_line_override=None,
            stdin_fd=FD_INHERIT,
            stdout_fd=FD_INHERIT,
            stderr_fd=FD_INHERIT,
        )

    def _exec_pathv_io(self, uc: Uc, path_ptr: int, argv_ptr: int, req_ptr: int) -> int:
        req_data: Optional[bytes]
        env_ptr: int
        stdin_fd: int
        stdout_fd: int
        stderr_fd: int
        env_line: str

        if req_ptr == 0:
            return u64_neg1()

        req_data = self._read_guest_bytes_exact(uc, req_ptr, 32)
        if req_data is None or len(req_data) != 32:
            return u64_neg1()

        env_ptr, stdin_fd, stdout_fd, stderr_fd = struct.unpack("<QQQQ", req_data)
        env_line = self._read_guest_cstring(uc, env_ptr, EXEC_ENV_LINE_MAX) if env_ptr != 0 else ""

        return self._spawn_path_common(
            uc,
            path_ptr,
            argv_ptr,
            0,
            return_pid=False,
            env_line_override=env_line,
            stdin_fd=stdin_fd,
            stdout_fd=stdout_fd,
            stderr_fd=stderr_fd,
        )

    def _spawn_path(self, uc: Uc, path_ptr: int) -> int:
        return self._spawn_path_common(
            uc,
            path_ptr,
            0,
            0,
            return_pid=True,
            env_line_override=None,
            stdin_fd=FD_INHERIT,
            stdout_fd=FD_INHERIT,
            stderr_fd=FD_INHERIT,
        )

    def _spawn_pathv(self, uc: Uc, path_ptr: int, argv_ptr: int, env_ptr: int) -> int:
        return self._spawn_path_common(
            uc,
            path_ptr,
            argv_ptr,
            env_ptr,
            return_pid=True,
            env_line_override=None,
            stdin_fd=FD_INHERIT,
            stdout_fd=FD_INHERIT,
            stderr_fd=FD_INHERIT,
        )

    def _spawn_path_common(
        self,
        uc: Uc,
        path_ptr: int,
        argv_ptr: int,
        env_ptr: int,
        *,
        return_pid: bool,
        env_line_override: Optional[str],
        stdin_fd: int,
        stdout_fd: int,
        stderr_fd: int,
    ) -> int:
        path = self._read_guest_cstring(uc, path_ptr, EXEC_PATH_MAX)
        guest_path = self._normalize_guest_path(path)
        argv_line = self._read_guest_cstring(uc, argv_ptr, EXEC_ARG_LINE_MAX) if argv_ptr != 0 else ""
        env_line = (
            env_line_override
            if env_line_override is not None
            else (self._read_guest_cstring(uc, env_ptr, EXEC_ENV_LINE_MAX) if env_ptr != 0 else "")
        )
        host_path = self._guest_to_host(guest_path, must_exist=True)
        child_stdio = self._build_child_stdio_map(int(stdin_fd), int(stdout_fd), int(stderr_fd))

        self.state.exec_requests = u64(self.state.exec_requests + 1)
        self.state.user_exec_requested = 1
        self.state.user_launch_tries = u64(self.state.user_launch_tries + 1)

        if child_stdio is None:
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        if host_path is None or not host_path.is_file():
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        if self.depth >= self.max_exec_depth:
            print(f"[WINE][WARN] exec depth exceeded: {guest_path}", file=sys.stderr)
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        parent_pid = self.state.get_current_pid()
        child_pid = self.state.alloc_pid(parent_pid)
        argv_items, env_items = self._build_child_exec_items(guest_path, argv_line, env_line)
        self.state.set_proc_cmdline(child_pid, argv_items, env_items)
        self.state.set_proc_fault(child_pid, 0, 0, 0, 0)

        child = CLeonOSWineNative(
            elf_path=host_path,
            rootfs=self.rootfs,
            guest_path_hint=guest_path,
            state=self.state,
            depth=self.depth + 1,
            max_exec_depth=self.max_exec_depth,
            no_kbd=True,
            verbose=self.verbose,
            top_level=False,
            pid=child_pid,
            ppid=parent_pid,
            argv_items=argv_items,
            env_items=env_items,
            inherited_fds=child_stdio,
        )
        child_ret = child.run()

        if child_ret is None:
            self.state.user_launch_fail = u64(self.state.user_launch_fail + 1)
            return u64_neg1()

        self.state.exec_success = u64(self.state.exec_success + 1)
        self.state.user_launch_ok = u64(self.state.user_launch_ok + 1)

        if guest_path.lower().startswith("/system/"):
            self.state.kelf_runs = u64(self.state.kelf_runs + 1)

        if return_pid:
            return child_pid

        return u64(child_ret)

    def _proc_argc(self) -> int:
        return self.state.proc_argc(self.state.get_current_pid())

    def _proc_argv(self, uc: Uc, index: int, out_ptr: int, out_size: int) -> int:
        return self._copy_proc_item_to_guest(
            uc,
            self.state.proc_argv_item(self.state.get_current_pid(), int(index)),
            out_ptr,
            out_size,
        )

    def _proc_envc(self) -> int:
        return self.state.proc_envc(self.state.get_current_pid())

    def _proc_env(self, uc: Uc, index: int, out_ptr: int, out_size: int) -> int:
        return self._copy_proc_item_to_guest(
            uc,
            self.state.proc_env_item(self.state.get_current_pid(), int(index)),
            out_ptr,
            out_size,
        )

    def _proc_last_signal(self) -> int:
        return self.state.proc_signal(self.state.get_current_pid())

    def _proc_fault_vector(self) -> int:
        return self.state.proc_fault_vector_value(self.state.get_current_pid())

    def _proc_fault_error(self) -> int:
        return self.state.proc_fault_error_value(self.state.get_current_pid())

    def _proc_fault_rip(self) -> int:
        return self.state.proc_fault_rip_value(self.state.get_current_pid())

    def _proc_count(self) -> int:
        return self.state.proc_count()

    def _proc_pid_at(self, uc: Uc, index: int, out_ptr: int) -> int:
        if out_ptr == 0:
            return 0

        pid = self.state.proc_pid_at(int(index))
        if pid is None:
            return 0

        return 1 if self._write_guest_bytes(uc, out_ptr, struct.pack("<Q", u64(pid))) else 0

    def _proc_snapshot(self, uc: Uc, pid: int, out_ptr: int, out_size: int) -> int:
        if out_ptr == 0 or out_size < (13 * 8 + PROC_PATH_MAX):
            return 0

        target = int(pid)
        state_value = self.state.proc_state_value(target)
        if state_value == 0:
            return 0

        path = self.state.proc_path_value(target)
        encoded_path = path.encode("utf-8", errors="replace")
        if len(encoded_path) >= PROC_PATH_MAX:
            encoded_path = encoded_path[: PROC_PATH_MAX - 1]
        path_buf = encoded_path + b"\x00" + (b"\x00" * (PROC_PATH_MAX - len(encoded_path) - 1))

        blob = struct.pack(
            "<13Q",
            u64(target),
            u64(self.state.proc_ppid(target)),
            u64(state_value),
            u64(self.state.proc_started_tick_value(target)),
            u64(self.state.proc_exited_tick_value(target)),
            u64(self.state.proc_exit_status_value(target)),
            u64(self.state.proc_runtime_ticks(target)),
            u64(self.state.proc_mem_bytes_value(target)),
            u64(self.state.proc_tty_index_value(target)),
            u64(self.state.proc_signal(target)),
            u64(self.state.proc_fault_vector_value(target)),
            u64(self.state.proc_fault_error_value(target)),
            u64(self.state.proc_fault_rip_value(target)),
        ) + path_buf

        return 1 if self._write_guest_bytes(uc, out_ptr, blob) else 0

    def _proc_kill(self, uc: Uc, pid: int, signal: int) -> int:
        target = int(pid)
        if target <= 0:
            return u64_neg1()

        current_state = self.state.proc_state_value(target)
        if current_state == 0:
            return u64_neg1()

        effective_signal = int(signal) & 0xFF
        if effective_signal == 0:
            effective_signal = SIGTERM

        self.state.set_proc_fault(target, effective_signal, 0, 0, 0)

        if current_state == PROC_STATE_EXITED:
            return 1

        if effective_signal == SIGCONT:
            if current_state == PROC_STATE_STOPPED:
                self.state.set_proc_pending(target)
            return 1

        if effective_signal == SIGSTOP and current_state in (PROC_STATE_PENDING, PROC_STATE_STOPPED):
            self.state.set_proc_stopped(target)
            return 1

        status = self._encode_signal_status(effective_signal, 0, 0)

        if target == self.state.get_current_pid():
            self._exit_requested = True
            self._exit_status = u64(status)
            if effective_signal == SIGSTOP:
                self.state.set_proc_stopped(target)
            uc.emu_stop()
            return 1

        if effective_signal == SIGSTOP:
            self.state.set_proc_stopped(target)
            return 1

        self.state.mark_exited(target, status)
        return 1

    def _wait_pid(self, uc: Uc, pid: int, out_ptr: int) -> int:
        wait_ret, status = self.state.wait_pid(int(pid))

        if wait_ret == 1 and out_ptr != 0:
            self._write_guest_bytes(uc, out_ptr, struct.pack("<Q", u64(status)))

        return int(wait_ret)

    def _request_exit(self, uc: Uc, status: int) -> int:
        self._exit_requested = True
        self._exit_status = u64(status)
        uc.emu_stop()
        return 1

    def _sleep_ticks(self, ticks: int) -> int:
        ticks = int(u64(ticks))

        if ticks == 0:
            return 0

        start = self.state.timer_ticks()

        while (self.state.timer_ticks() - start) < ticks:
            time.sleep(0.001)

        return self.state.timer_ticks() - start

    def _yield_once(self) -> int:
        time.sleep(0)
        return self.state.timer_ticks()

    def _fd_open(self, uc: Uc, path_ptr: int, flags: int, mode: int) -> int:
        _ = mode
        guest_path = self._normalize_guest_path(self._read_guest_cstring(uc, path_ptr, EXEC_PATH_MAX))
        open_flags = int(u64(flags))
        lower_path = guest_path.lower()
        fd_slot: int
        entry: FDEntry

        if not guest_path.startswith("/"):
            return u64_neg1()

        if not self._fd_access_mode_valid(open_flags):
            return u64_neg1()

        if ((open_flags & O_TRUNC) != 0 or (open_flags & O_APPEND) != 0) and not self._fd_can_write(open_flags):
            return u64_neg1()

        fd_slot = self._fd_find_free()
        if fd_slot < 0:
            return u64_neg1()

        if lower_path == "/dev/tty":
            entry = FDEntry(kind="tty", flags=open_flags, offset=0, tty_index=self._tty_index)
            self._fds[fd_slot] = entry
            return fd_slot

        if lower_path == "/dev/null":
            entry = FDEntry(kind="dev_null", flags=open_flags, offset=0, tty_index=self._tty_index)
            self._fds[fd_slot] = entry
            return fd_slot

        if lower_path == "/dev/zero":
            entry = FDEntry(kind="dev_zero", flags=open_flags, offset=0, tty_index=self._tty_index)
            self._fds[fd_slot] = entry
            return fd_slot

        if lower_path == "/dev/random":
            entry = FDEntry(kind="dev_random", flags=open_flags, offset=0, tty_index=self._tty_index)
            self._fds[fd_slot] = entry
            return fd_slot

        host_path = self._guest_to_host(guest_path, must_exist=False)
        if host_path is None:
            return u64_neg1()

        try:
            if not host_path.exists():
                if (open_flags & O_CREAT) == 0 or not self._fd_can_write(open_flags):
                    return u64_neg1()
                host_path.parent.mkdir(parents=True, exist_ok=True)
                host_path.write_bytes(b"")

            if host_path.is_dir():
                return u64_neg1()

            if (open_flags & O_TRUNC) != 0:
                host_path.write_bytes(b"")

            offset = int(host_path.stat().st_size) if (open_flags & O_APPEND) != 0 else 0
        except Exception:
            return u64_neg1()

        entry = FDEntry(kind="file", flags=open_flags, offset=offset, path=str(host_path), tty_index=self._tty_index)
        self._fds[fd_slot] = entry
        return fd_slot

    def _fd_read(self, uc: Uc, fd: int, out_ptr: int, size: int) -> int:
        req = int(u64(size))
        entry = self._fd_lookup(int(fd))
        data: bytes

        if req == 0:
            return 0

        if out_ptr == 0:
            return u64_neg1()

        if entry is None or not self._fd_can_read(entry.flags):
            return u64_neg1()

        req = min(req, MAX_IO_READ)

        if entry.kind == "tty":
            out = bytearray()
            while len(out) < req:
                key = self.state.pop_key()
                if key is None:
                    break
                out.append(key & 0xFF)
            data = bytes(out)
        elif entry.kind == "dev_null":
            return 0
        elif entry.kind == "dev_zero":
            data = b"\x00" * req
        elif entry.kind == "dev_random":
            data = os.urandom(req)
        elif entry.kind == "file":
            try:
                with open(entry.path, "rb") as fh:
                    fh.seek(entry.offset)
                    data = fh.read(req)
            except Exception:
                return u64_neg1()
        else:
            return u64_neg1()

        if len(data) == 0:
            return 0

        if not self._write_guest_bytes(uc, int(out_ptr), data):
            return u64_neg1()

        entry.offset += len(data)
        return len(data)

    def _fd_write(self, uc: Uc, fd: int, buf_ptr: int, size: int) -> int:
        req = int(u64(size))
        entry = self._fd_lookup(int(fd))
        data: Optional[bytes]
        write_pos: int

        if req == 0:
            return 0

        if buf_ptr == 0:
            return u64_neg1()

        if entry is None or not self._fd_can_write(entry.flags):
            return u64_neg1()

        req = min(req, MAX_IO_READ)
        data = self._read_guest_bytes_exact(uc, int(buf_ptr), req)
        if data is None:
            return u64_neg1()

        if entry.kind == "tty":
            self._host_write_bytes(data)
            entry.offset += len(data)
            return len(data)

        if entry.kind in ("dev_null", "dev_zero", "dev_random"):
            entry.offset += len(data)
            return len(data)

        if entry.kind != "file":
            return u64_neg1()

        try:
            host_path = Path(entry.path)
            if not host_path.exists():
                if (entry.flags & O_CREAT) == 0 or not self._fd_can_write(entry.flags):
                    return u64_neg1()
                host_path.parent.mkdir(parents=True, exist_ok=True)
                host_path.write_bytes(b"")

            with open(host_path, "r+b") as fh:
                write_pos = int(entry.offset)
                fh.seek(write_pos)
                fh.write(data)
        except Exception:
            return u64_neg1()

        entry.offset += len(data)
        return len(data)

    def _fd_close(self, fd: int) -> int:
        key = int(fd)
        if key not in self._fds:
            return u64_neg1()
        del self._fds[key]
        return 0

    def _fd_dup(self, fd: int) -> int:
        src = self._fd_lookup(int(fd))
        if src is None:
            return u64_neg1()

        slot = self._fd_find_free()
        if slot < 0:
            return u64_neg1()

        self._fds[slot] = self._clone_fd_entry(src)
        return slot

    @staticmethod
    def _truncate_item_text(text: str, max_bytes: int = EXEC_ITEM_MAX) -> str:
        if max_bytes <= 1:
            return ""
        encoded = (text or "").encode("utf-8", errors="replace")
        if len(encoded) >= max_bytes:
            encoded = encoded[: max_bytes - 1]
        return encoded.decode("utf-8", errors="ignore")

    @staticmethod
    def _parse_whitespace_items(line: str, max_count: int) -> List[str]:
        out: List[str] = []
        i = 0
        text = line or ""
        length = len(text)

        while i < length:
            while i < length and text[i] in (" ", "\t", "\r", "\n"):
                i += 1
            if i >= length:
                break
            start = i
            while i < length and text[i] not in (" ", "\t", "\r", "\n"):
                i += 1
            out.append(text[start:i])
            if len(out) >= max_count:
                break

        return out

    @staticmethod
    def _parse_env_items(line: str, max_count: int) -> List[str]:
        out: List[str] = []
        i = 0
        text = line or ""
        length = len(text)

        while i < length:
            while i < length and text[i] in (" ", "\t", ";", "\r", "\n"):
                i += 1
            if i >= length:
                break

            start = i
            while i < length and text[i] not in (";", "\r", "\n"):
                i += 1
            value = text[start:i].rstrip(" \t")
            if value:
                out.append(value)
                if len(out) >= max_count:
                    break

        return out

    @classmethod
    def _prepare_exec_items(cls, path: str, argv_items: List[str], env_items: List[str]) -> Tuple[List[str], List[str]]:
        normalized_path = cls._normalize_guest_path(path)

        prepared_argv: List[str] = []
        for item in argv_items[:EXEC_MAX_ARGS]:
            value = cls._truncate_item_text(item)
            if value:
                prepared_argv.append(value)

        if not prepared_argv:
            prepared_argv = [cls._truncate_item_text(normalized_path)]
        elif prepared_argv[0] == "":
            prepared_argv[0] = cls._truncate_item_text(normalized_path)

        prepared_env: List[str] = []
        for item in env_items[:EXEC_MAX_ENVS]:
            value = cls._truncate_item_text(item)
            if value:
                prepared_env.append(value)

        return prepared_argv[:EXEC_MAX_ARGS], prepared_env[:EXEC_MAX_ENVS]

    @classmethod
    def _build_child_exec_items(cls, guest_path: str, argv_line: str, env_line: str) -> Tuple[List[str], List[str]]:
        argv_items = [guest_path]
        argv_items.extend(cls._parse_whitespace_items(argv_line or "", EXEC_MAX_ARGS - 1))
        env_items = cls._parse_env_items(env_line or "", EXEC_MAX_ENVS)
        return cls._prepare_exec_items(guest_path, argv_items, env_items)

    def _copy_proc_item_to_guest(self, uc: Uc, item: Optional[str], out_ptr: int, out_size: int) -> int:
        if out_ptr == 0 or out_size == 0 or item is None:
            return 0

        max_out = int(min(int(out_size), EXEC_ITEM_MAX))
        if max_out <= 0:
            return 0

        encoded = self._truncate_item_text(item, max_out + 1).encode("utf-8", errors="replace")
        if len(encoded) >= max_out:
            encoded = encoded[: max_out - 1]

        return 1 if self._write_guest_bytes(uc, out_ptr, encoded + b"\x00") else 0

    @staticmethod
    def _signal_from_vector(vector: int) -> int:
        if vector in (0, 16, 19):
            return 8
        if vector == 6:
            return 4
        if vector == 3:
            return 5
        if vector in (10, 11, 12, 13, 14, 17):
            return 11
        return 6

    @staticmethod
    def _encode_signal_status(signal: int, vector: int, error_code: int) -> int:
        return u64(
            EXEC_STATUS_SIGNAL_FLAG
            | (int(signal) & 0xFF)
            | ((int(vector) & 0xFF) << 8)
            | ((int(error_code) & 0xFFFF) << 16)
        )

    def _status_from_uc_error(self, uc: Uc, exc: UcError) -> int:
        errno = int(getattr(exc, "errno", 0))
        vector = 13
        error_code = 0

        if errno in (UC_ERR_READ_UNMAPPED, UC_ERR_WRITE_UNMAPPED, UC_ERR_FETCH_UNMAPPED):
            vector = 14
            if errno == UC_ERR_WRITE_UNMAPPED:
                error_code = 0x2
            elif errno == UC_ERR_FETCH_UNMAPPED:
                error_code = 0x10
        elif errno in (UC_ERR_READ_PROT, UC_ERR_WRITE_PROT, UC_ERR_FETCH_PROT):
            vector = 14
            error_code = 0x1
            if errno == UC_ERR_WRITE_PROT:
                error_code |= 0x2
            elif errno == UC_ERR_FETCH_PROT:
                error_code |= 0x10
        elif errno == UC_ERR_INSN_INVALID:
            vector = 6
            error_code = 0

        rip = 0
        try:
            rip = self._reg_read(uc, UC_X86_REG_RIP)
        except Exception:
            rip = 0

        signal = self._signal_from_vector(vector)
        self.state.set_proc_fault(self.pid, signal, vector, error_code, rip)
        return self._encode_signal_status(signal, vector, error_code)

    def _guest_to_host(self, guest_path: str, *, must_exist: bool) -> Optional[Path]:
        norm = self._normalize_guest_path(guest_path)
        if norm == "/":
            return self.rootfs if (not must_exist or self.rootfs.exists()) else None

        current = self.rootfs
        for part in [p for p in norm.split("/") if p]:
            candidate = current / part
            if candidate.exists():
                current = candidate
                continue

            if current.exists() and current.is_dir():
                match = self._find_case_insensitive(current, part)
                if match is not None:
                    current = match
                    continue

            current = candidate

        if must_exist and not current.exists():
            return None
        return current

    @staticmethod
    def _find_case_insensitive(parent: Path, name: str) -> Optional[Path]:
        target = name.lower()
        try:
            for entry in parent.iterdir():
                if entry.name.lower() == target:
                    return entry
        except Exception:
            return None
        return None

    @staticmethod
    def _normalize_guest_path(path: str) -> str:
        p = (path or "").replace("\\", "/").strip()
        if not p:
            return "/"
        if not p.startswith("/"):
            p = "/" + p

        parts = []
        for token in p.split("/"):
            if token in ("", "."):
                continue
            if token == "..":
                if parts:
                    parts.pop()
                continue
            parts.append(token)

        return "/" + "/".join(parts)

    @staticmethod
    def _list_children(dir_path: Path) -> List[str]:
        try:
            names = [entry.name for entry in dir_path.iterdir() if not entry.name.startswith(".")]
        except Exception:
            return []
        names.sort(key=lambda x: x.lower())
        return names


def resolve_rootfs(path_arg: Optional[str]) -> Path:
    if path_arg:
        root = Path(path_arg).expanduser().resolve()
        if not root.exists() or not root.is_dir():
            raise FileNotFoundError(f"rootfs not found: {root}")
        return root

    candidates = [
        Path("build/x86_64/ramdisk_root"),
        Path("ramdisk"),
    ]
    for candidate in candidates:
        if candidate.exists() and candidate.is_dir():
            return candidate.resolve()

    raise FileNotFoundError("rootfs not found; pass --rootfs")


def _guest_to_host_for_resolve(rootfs: Path, guest_path: str) -> Optional[Path]:
    norm = CLeonOSWineNative._normalize_guest_path(guest_path)
    if norm == "/":
        return rootfs

    current = rootfs
    for part in [p for p in norm.split("/") if p]:
        candidate = current / part
        if candidate.exists():
            current = candidate
            continue

        if current.exists() and current.is_dir():
            match = None
            for entry in current.iterdir():
                if entry.name.lower() == part.lower():
                    match = entry
                    break
            if match is not None:
                current = match
                continue

        current = candidate

    return current if current.exists() else None


def resolve_elf_target(elf_arg: str, rootfs: Path) -> Tuple[Path, str]:
    host_candidate = Path(elf_arg).expanduser()
    if host_candidate.exists():
        host_path = host_candidate.resolve()
        try:
            rel = host_path.relative_to(rootfs)
            guest_path = "/" + rel.as_posix()
        except ValueError:
            guest_path = "/" + host_path.name
        return host_path, guest_path

    guest_path = CLeonOSWineNative._normalize_guest_path(elf_arg)
    host_path = _guest_to_host_for_resolve(rootfs, guest_path)
    if host_path is None:
        raise FileNotFoundError(f"ELF not found as host path or guest path: {elf_arg}")
    return host_path.resolve(), guest_path
