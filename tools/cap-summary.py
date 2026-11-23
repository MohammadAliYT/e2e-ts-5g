#!/usr/bin/env python3
# Capacity test summary: one line per traffic step of one cap-test.sh run.
# The script finds the iperf3 file of each step in the step result files (<run>/ue1-*.txt).
# For each step it takes the controller CSV rows from start + 3 s to start + STEP_S
# and prints the means per DU: cell PRB use DL/UL, and per UE: throughput DL/UL
# (controller), MCS DL/UL, BLER DL/UL, DL head-of-line delay, DL buffer.
# Usage: python3 -I tools/cap-summary.py [run folder]   (default: results/cap/latest)
import csv, glob, os, re, sys, time
from statistics import mean


def num(v):
    """Return the value as float, or None if the field is empty."""
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def avg(rows, key):
    """Return the mean of one column, or NaN if there are no values."""
    vals = [num(r[key]) for r in rows if num(r[key]) is not None]
    return mean(vals) if vals else float("nan")


def settings(run):
    """Return the run settings (settings.txt). STEP_S is 15 if the file is missing."""
    s = {"STEP_S": "15"}
    path = os.path.join(run, "settings.txt")
    if os.path.exists(path):
        for line in open(path):
            key, _, val = line.strip().partition("=")
            if key:
                s[key] = val
    return s


def steps(run):
    """Return (result name, iperf3 file) for each step, sorted by start time."""
    found = []
    for txt in glob.glob(os.path.join(run, "ue1-*.txt")):
        name = os.path.basename(txt)[:-4]
        for line in open(txt):
            m = re.search(r"(/\S+\.json)", line)
            if m:
                found.append((name, m.group(1)))
    return sorted(found, key=lambda s: os.path.basename(s[1]))


def ue_text(rows, ue):
    """Return the summary of one UE, or an empty text if the UE has no rows."""
    if not rows:
        return ""
    return (f" ue{ue} {avg(rows, 'dl_mbps'):5.1f}/{avg(rows, 'ul_mbps'):4.1f} Mbit/s"
            f" mcs {avg(rows, 'dl_mcs'):4.1f}/{avg(rows, 'ul_mcs'):4.1f}"
            f" bler {avg(rows, 'dl_bler'):.2f}/{avg(rows, 'ul_bler'):.2f}"
            f" hol {avg(rows, 'dl_hol_us_mean') / 1000:5.1f} ms"
            f" buf {avg(rows, 'dl_buf_bytes_mean') / 1000:6.1f} kB")


def main(run):
    step_s = float(settings(run)["STEP_S"])
    rows = list(csv.DictReader(open(os.path.join(run, "ctrl.csv"))))
    print("case time   dir rate | per DU: cell PRB DL/UL %, per UE: DL/UL, hol + buf = DL queue")
    for name, path in steps(run):
        m = re.search(r"(\d{8}-\d{6})-ue\d-(dl|ul)-(\w+)-", os.path.basename(path))
        if not m:
            continue
        stamp, direction, rate = m.groups()
        t0 = time.mktime(time.strptime(stamp, "%Y%m%d-%H%M%S"))
        win = [r for r in rows if t0 + 3 <= (num(r["t_unix"]) or 0) <= t0 + step_s]
        line = f"{name[-1]:>4} {stamp[9:]} {direction} {rate:>4} |"
        for du in sorted({r["du"] for r in win}):
            cell = [r for r in win if r["du"] == du]
            line += f" DU{du} PRB {avg(cell, 'cell_prb_dl'):3.0f}/{avg(cell, 'cell_prb_ul'):3.0f} %"
            for ue in ("1", "2"):
                line += ue_text([r for r in cell if r["cu_ue_id"] == ue], ue)
            line += " |"
        print(line)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "results/cap/latest")