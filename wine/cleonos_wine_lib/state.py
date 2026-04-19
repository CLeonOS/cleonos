from __future__ import annotations

import collections
import threading
import time
from dataclasses import dataclass, field
from typing import Deque, Dict, List, Optional, Tuple

from .constants import PROC_STATE_EXITED, PROC_STATE_PENDING, PROC_STATE_RUNNING, PROC_STATE_STOPPED, u64


@dataclass
class SharedKernelState:
    start_ns: int = field(default_factory=time.monotonic_ns)
    task_count: int = 5
    current_task: int = 0
    service_count: int = 7
    service_ready: int = 7
    context_switches: int = 0
    kelf_count: int = 2
    kelf_runs: int = 0
    exec_requests: int = 0
    exec_success: int = 0
    user_shell_ready: int = 1
    user_exec_requested: int = 0
    user_launch_tries: int = 0
    user_launch_ok: int = 0
    user_launch_fail: int = 0
    tty_count: int = 4
    tty_active: int = 0
    kbd_queue: Deque[int] = field(default_factory=collections.deque)
    kbd_lock: threading.Lock = field(default_factory=threading.Lock)
    kbd_queue_cap: int = 256
    kbd_drop_count: int = 0
    kbd_push_count: int = 0
    kbd_pop_count: int = 0
    kbd_hotkey_switches: int = 0
    log_journal_cap: int = 256
    log_journal: Deque[str] = field(default_factory=lambda: collections.deque(maxlen=256))
    fs_write_max: int = 16 * 1024 * 1024

    # syscall stats
    stats_lock: threading.Lock = field(default_factory=threading.Lock)
    stats_total_calls: int = 0
    stats_id_total: Dict[int, int] = field(default_factory=dict)
    stats_recent_window_cap: int = 256
    stats_recent_ring: Deque[int] = field(default_factory=lambda: collections.deque(maxlen=256))

    # process table
    proc_lock: threading.Lock = field(default_factory=threading.Lock)
    proc_next_pid: int = 1
    proc_current_pid: int = 0
    proc_parents: Dict[int, int] = field(default_factory=dict)
    proc_status: Dict[int, Optional[int]] = field(default_factory=dict)
    proc_state: Dict[int, int] = field(default_factory=dict)
    proc_started_tick: Dict[int, int] = field(default_factory=dict)
    proc_exited_tick: Dict[int, int] = field(default_factory=dict)
    proc_mem_bytes: Dict[int, int] = field(default_factory=dict)
    proc_tty_index: Dict[int, int] = field(default_factory=dict)
    proc_path: Dict[int, str] = field(default_factory=dict)
    proc_argv: Dict[int, List[str]] = field(default_factory=dict)
    proc_env: Dict[int, List[str]] = field(default_factory=dict)
    proc_last_signal: Dict[int, int] = field(default_factory=dict)
    proc_fault_vector: Dict[int, int] = field(default_factory=dict)
    proc_fault_error: Dict[int, int] = field(default_factory=dict)
    proc_fault_rip: Dict[int, int] = field(default_factory=dict)

    def timer_ticks(self) -> int:
        return (time.monotonic_ns() - self.start_ns) // 1_000_000

    def record_syscall(self, sid: int) -> None:
        key = int(u64(sid))

        with self.stats_lock:
            self.stats_total_calls = int(u64(self.stats_total_calls + 1))
            self.stats_id_total[key] = int(u64(self.stats_id_total.get(key, 0) + 1))
            self.stats_recent_ring.append(key)

    def stats_total(self) -> int:
        with self.stats_lock:
            return int(self.stats_total_calls)

    def stats_id_count(self, sid: int) -> int:
        key = int(u64(sid))
        with self.stats_lock:
            return int(self.stats_id_total.get(key, 0))

    def stats_recent_window(self) -> int:
        with self.stats_lock:
            return len(self.stats_recent_ring)

    def stats_recent_id_count(self, sid: int) -> int:
        key = int(u64(sid))
        with self.stats_lock:
            return sum(1 for item in self.stats_recent_ring if item == key)

    def push_key(self, key: int) -> None:
        with self.kbd_lock:
            if len(self.kbd_queue) >= self.kbd_queue_cap:
                self.kbd_queue.popleft()
                self.kbd_drop_count = u64(self.kbd_drop_count + 1)
            self.kbd_queue.append(key & 0xFF)
            self.kbd_push_count = u64(self.kbd_push_count + 1)

    def pop_key(self) -> Optional[int]:
        with self.kbd_lock:
            if not self.kbd_queue:
                return None
            self.kbd_pop_count = u64(self.kbd_pop_count + 1)
            return self.kbd_queue.popleft()

    def buffered_count(self) -> int:
        with self.kbd_lock:
            return len(self.kbd_queue)

    def log_journal_push(self, text: str) -> None:
        if text is None:
            return

        normalized = text.replace("\r", "")
        lines = normalized.split("\n")

        for line in lines:
            if len(line) > 255:
                line = line[:255]
            self.log_journal.append(line)

    def log_journal_count(self) -> int:
        return len(self.log_journal)

    def log_journal_read(self, index_from_oldest: int) -> Optional[str]:
        if index_from_oldest < 0 or index_from_oldest >= len(self.log_journal):
            return None
        return list(self.log_journal)[index_from_oldest]

    def alloc_pid(self, ppid: int) -> int:
        now = self.timer_ticks()

        with self.proc_lock:
            pid = int(self.proc_next_pid)

            if pid == 0:
                pid = 1

            self.proc_next_pid = int(u64(pid + 1))

            if self.proc_next_pid == 0:
                self.proc_next_pid = 1

            self.proc_parents[pid] = int(ppid)
            self.proc_status[pid] = None
            self.proc_state[pid] = PROC_STATE_PENDING
            self.proc_started_tick[pid] = now
            self.proc_exited_tick[pid] = 0
            self.proc_mem_bytes[pid] = 0
            self.proc_tty_index[pid] = int(self.tty_active)
            self.proc_path[pid] = ""
            self.proc_argv[pid] = []
            self.proc_env[pid] = []
            self.proc_last_signal[pid] = 0
            self.proc_fault_vector[pid] = 0
            self.proc_fault_error[pid] = 0
            self.proc_fault_rip[pid] = 0
            return pid

    def set_current_pid(self, pid: int) -> None:
        with self.proc_lock:
            self.proc_current_pid = int(pid)

    def get_current_pid(self) -> int:
        with self.proc_lock:
            return int(self.proc_current_pid)

    def set_proc_running(self, pid: int, path: Optional[str], tty_index: int) -> None:
        if pid <= 0:
            return

        now = self.timer_ticks()

        with self.proc_lock:
            if pid not in self.proc_status:
                return

            self.proc_state[pid] = PROC_STATE_RUNNING
            if self.proc_started_tick.get(pid, 0) == 0:
                self.proc_started_tick[pid] = now
            self.proc_tty_index[pid] = int(tty_index)
            if path:
                self.proc_path[pid] = str(path)

    def mark_exited(self, pid: int, status: int) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            self.proc_status[int(pid)] = int(u64(status))
            self.proc_state[int(pid)] = PROC_STATE_EXITED
            self.proc_exited_tick[int(pid)] = self.timer_ticks()

    def set_proc_stopped(self, pid: int) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            if pid not in self.proc_status:
                return
            if self.proc_state.get(pid, PROC_STATE_PENDING) != PROC_STATE_EXITED:
                self.proc_state[pid] = PROC_STATE_STOPPED

    def set_proc_pending(self, pid: int) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            if pid not in self.proc_status:
                return
            if self.proc_state.get(pid, PROC_STATE_PENDING) != PROC_STATE_EXITED:
                self.proc_state[pid] = PROC_STATE_PENDING

    def wait_pid(self, pid: int) -> Tuple[int, int]:
        with self.proc_lock:
            if pid not in self.proc_status:
                return int(u64(-1)), 0

            status = self.proc_status[pid]

            if status is None:
                return 0, 0

            return 1, int(status)

    def set_proc_cmdline(self, pid: int, argv: List[str], env: List[str]) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            if pid not in self.proc_status:
                return
            self.proc_argv[pid] = [str(item) for item in argv]
            self.proc_env[pid] = [str(item) for item in env]
            if argv:
                self.proc_path[pid] = str(argv[0])

    def set_proc_fault(self, pid: int, signal: int, vector: int, error_code: int, rip: int) -> None:
        if pid <= 0:
            return

        with self.proc_lock:
            if pid not in self.proc_status:
                return
            self.proc_last_signal[pid] = int(u64(signal))
            self.proc_fault_vector[pid] = int(u64(vector))
            self.proc_fault_error[pid] = int(u64(error_code))
            self.proc_fault_rip[pid] = int(u64(rip))

    def proc_count(self) -> int:
        with self.proc_lock:
            return len(self.proc_status)

    def proc_pid_at(self, index: int) -> Optional[int]:
        if index < 0:
            return None

        with self.proc_lock:
            ordered = sorted(self.proc_status.keys())
            if index >= len(ordered):
                return None
            return int(ordered[index])

    def proc_state_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_state.get(int(pid), 0))

    def proc_ppid(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_parents.get(int(pid), 0))

    def proc_started_tick_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_started_tick.get(int(pid), 0))

    def proc_exited_tick_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_exited_tick.get(int(pid), 0))

    def proc_exit_status_value(self, pid: int) -> int:
        with self.proc_lock:
            value = self.proc_status.get(int(pid))
            return int(value) if value is not None else 0

    def proc_runtime_ticks(self, pid: int) -> int:
        with self.proc_lock:
            start = int(self.proc_started_tick.get(int(pid), 0))
            state = int(self.proc_state.get(int(pid), 0))
            end = int(self.proc_exited_tick.get(int(pid), 0))

        if start == 0:
            return 0

        if state == PROC_STATE_EXITED and end >= start:
            return end - start

        now = self.timer_ticks()
        return 0 if now < start else now - start

    def proc_mem_bytes_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_mem_bytes.get(int(pid), 0))

    def proc_tty_index_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_tty_index.get(int(pid), 0))

    def proc_path_value(self, pid: int) -> str:
        with self.proc_lock:
            return str(self.proc_path.get(int(pid), ""))

    def proc_argc(self, pid: int) -> int:
        with self.proc_lock:
            return len(self.proc_argv.get(int(pid), []))

    def proc_argv_item(self, pid: int, index: int) -> Optional[str]:
        with self.proc_lock:
            values = self.proc_argv.get(int(pid), [])
            if index < 0 or index >= len(values):
                return None
            return values[index]

    def proc_envc(self, pid: int) -> int:
        with self.proc_lock:
            return len(self.proc_env.get(int(pid), []))

    def proc_env_item(self, pid: int, index: int) -> Optional[str]:
        with self.proc_lock:
            values = self.proc_env.get(int(pid), [])
            if index < 0 or index >= len(values):
                return None
            return values[index]

    def proc_signal(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_last_signal.get(int(pid), 0))

    def proc_fault_vector_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_fault_vector.get(int(pid), 0))

    def proc_fault_error_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_fault_error.get(int(pid), 0))

    def proc_fault_rip_value(self, pid: int) -> int:
        with self.proc_lock:
            return int(self.proc_fault_rip.get(int(pid), 0))
