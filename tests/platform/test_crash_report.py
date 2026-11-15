"""Crashes crash_report_probe each supported way and checks the report it leaves in CrashReports.

Usage: test_crash_report.py <crash_report_probe.exe>
"""
import os
import shutil
import subprocess
import sys
import time

CASES = {
    # kind: (text the report must contain, whether a dump is required)
    "access-violation": ("access violation", True),
    "stack-overflow": ("stack overflow", True),
    "cpp-exception": ("runtime_error", True),
    "abort": ("abort() was called", True),
    "pure-call": ("pure virtual function call", True),
    "terminate": ("std::terminate()", True),
    "heap-corruption": ("heap corruption", True),
    # The dump would wait for the heap lock the crashed thread holds: the text report is enough.
    "heap-locked": ("access violation", False),
}
DUMP_SIGNATURE = b"MDMP"


def run(probe, kind):
    folder = os.path.join(os.path.dirname(probe), "CrashReports")
    shutil.rmtree(folder, ignore_errors=True)
    env = dict(os.environ, MU_CRASH_REPORT_NO_DIALOG="1")
    result = subprocess.run([probe, kind], env=env, timeout=120, capture_output=True)
    if result.returncode == 0:
        return f"{kind}: the probe did not crash"
    texts = sorted(n for n in os.listdir(folder) if n.endswith(".txt")) if os.path.isdir(folder) else []
    if len(texts) != 1:
        return f"{kind}: expected one report, found {texts}"
    with open(os.path.join(folder, texts[0]), encoding="utf-8") as f:
        text = f.read()
    expected, dump_expected = CASES[kind]
    if expected not in text or "crash_report_probe.exe+0x" not in text:
        return f"{kind}: report lacks {expected!r} or a probe address:\n{text}"
    dump = os.path.join(folder, texts[0][:-4] + ".dmp")
    if dump_expected:
        if not os.path.isfile(dump):
            return f"{kind}: no dump next to the report:\n{text}"
        with open(dump, "rb") as f:
            if f.read(4) != DUMP_SIGNATURE:
                return f"{kind}: the dump is not a minidump"
    elif os.path.isfile(dump):
        return f"{kind}: a half-written dump was left next to the text-only report"
    return None


def check_notice(probe):
    """A crash with the dialog wanted starts a second probe with --crash-notice; it writes the
    dialog's text to MU_CRASH_REPORT_NOTICE_LOG instead of showing it."""
    folder = os.path.join(os.path.dirname(probe), "CrashReports")
    shutil.rmtree(folder, ignore_errors=True)
    log = os.path.join(os.path.dirname(probe), "crash-notice.log")
    if os.path.exists(log):
        os.remove(log)
    env = {k: v for k, v in os.environ.items() if k not in ("MU_CRASH_REPORT_NO_DIALOG", "MU_CONTROL_SOCKET")}
    env["MU_CRASH_REPORT_NOTICE_LOG"] = log
    started = time.monotonic()
    result = subprocess.run([probe, "access-violation"], env=env, timeout=120, capture_output=True)
    if result.returncode == 0:
        return "notice: the probe did not crash"
    if time.monotonic() - started > 15:
        return "notice: the crashed probe waited instead of closing"
    deadline = time.monotonic() + 20
    while not os.path.isfile(log) and time.monotonic() < deadline:
        time.sleep(0.2)
    if not os.path.isfile(log):
        return "notice: no --crash-notice process wrote its text"
    time.sleep(0.5)
    with open(log, "rb") as f:
        notice = f.read().decode("utf-16-le")
    os.remove(log)
    texts = sorted(n for n in os.listdir(folder) if n.endswith(".txt")) if os.path.isdir(folder) else []
    if len(texts) != 1 or texts[0] not in notice or texts[0][:-4] + ".dmp" not in notice or "private" not in notice:
        return f"notice: the dialog text does not name the report {texts}:\n{notice}"
    return None


def main():
    probe = sys.argv[1]
    if subprocess.run([probe, "none"], timeout=60).returncode != 0:
        print("the probe failed without crashing")
        return 1
    failures = [problem for problem in (run(probe, kind) for kind in CASES) if problem]
    notice_problem = check_notice(probe)
    if notice_problem:
        failures.append(notice_problem)
    shutil.rmtree(os.path.join(os.path.dirname(probe), "CrashReports"), ignore_errors=True)
    for problem in failures:
        print(problem)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
