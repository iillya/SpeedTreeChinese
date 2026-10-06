"""Isolated release-DLL lifecycle acceptance; preserves command/output/exit evidence."""
import argparse
import datetime
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", type=Path)
    parser.add_argument("major", choices=("5", "6"))
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    version = sdk.parent.name
    output = ROOT / "build" / "acceptance-20260928" / ("lifecycle-qt" + version)
    output.mkdir(parents=True, exist_ok=True)
    results = []

    def run(name, command, env, timeout=15):
        started = time.monotonic()
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, timeout=timeout)
            code, raw = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, raw = -1, (error.stdout or b"") + (error.stderr or b"") + b"\nTIMEOUT"
        log = output / (name + ".log")
        log.write_bytes(raw)
        record = {"case": name, "command": command, "sdk": str(sdk), "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  "exit_code": code, "elapsed_seconds": round(time.monotonic()-started, 3),
                  "status": "PASS" if code == 0 and b"PASS Qt" in raw else "FAIL", "log": str(log.relative_to(ROOT))}
        results.append(record)
        print(record["status"], name, code, flush=True)
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        return record["status"] == "PASS"

    environment = os.environ.copy()
    if not run("build-and-late", ["cmd.exe", "/d", "/c", str(ROOT / "scripts/test_runtime_load.bat"),
                                 str(sdk), args.major, "late", "0"], environment, 60):
        raise SystemExit(1)
    environment.update({"PATH": str(sdk / "bin") + os.pathsep + environment["PATH"],
                        "QT_PLUGIN_PATH": str(sdk / "plugins"), "QT_QPA_PLATFORM": "offscreen",
                        "QT_QPA_FONTDIR": os.path.join(environment["WINDIR"], "Fonts"),
                        "MARI_CHINESE_DPI_ENABLED": "0", "MARI_CHINESE_FALLBACK_POLLING": "0"})
    project = ROOT.name
    suffix = "-qt5" if project == "SpeedTreeChinese" and args.major == "5" else ""
    prefix = [str(ROOT / ("build/runtime-load-qt" + args.major) / "test.exe"),
              str(ROOT / "build/out" / (project + "Hook" + suffix + ".dll")),
              "mari_chinese_language" if project == "MariChinese" else "speedtree_lang"]
    run("early", prefix + ["early", "250"], environment)
    run("delayed-gui", prefix + ["delayed-gui", "0"], environment)
    for delay in (0, 50, 250):
        run("no-app-exit-" + str(delay), prefix + ["no-app-exit", str(delay)], environment)
    for delay in (0, 1, 10, 50, 90, 110, 190, 250):
        for iteration in range(5):
            run("early-shutdown-%d-%d" % (delay, iteration), prefix + ["early-shutdown", str(delay)], environment)
    raise SystemExit(int(any(r["status"] != "PASS" for r in results)))


if __name__ == "__main__":
    main()

