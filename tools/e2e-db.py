#!/usr/bin/env python3
"""Show what an xApp received over E2, from FlexRIC's SQLite xApp database.
Usage: python3 -I tools/e2e-db.py [db file]     (default: newest /tmp/xapp_db_*)
Prints the tables, the MAC table's columns, then per UE (RNTI) and 5 s interval the mean of the
MAC service-model values the controller will use."""

import glob
import os
import sqlite3
import sys

dbs = [
    f
    for f in glob.glob("/tmp/xapp_db_*")
    if not f.endswith(("-journal", "-wal", "-shm"))
]
db = sys.argv[1] if len(sys.argv) > 1 else max(dbs, key=os.path.getmtime)
con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
tables = [
    r[0] for r in con.execute("SELECT name FROM sqlite_master WHERE type='table'")
]
print(f"db: {db}")
for t in tables:
    n = con.execute('SELECT COUNT(*) FROM "%s"' % t).fetchone()[0]
    print(f"  {t:20s} {n} rows")

mac = next((t for t in tables if "mac" in t.lower()), None)
if mac is None:
    sys.exit("no MAC table")
cols = [r[1] for r in con.execute(f'PRAGMA table_info("{mac}")')]
print(f"\n{mac} columns: {', '.join(cols)}")

want = [
    "dl_mcs1",
    "dl_bler",
    "ul_mcs1",
    "ul_bler",
    "pusch_snr",
    "pucch_snr",
    "wb_cqi",
    "dl_sched_rb",
    "ul_sched_rb",
]
ts = next((c for c in cols if "tstamp" in c.lower()), None)
node = (
    "nb_id" if "nb_id" in cols else None
)  # DU1 = 3584, DU2 = 3585; cu_du_id is the same for both
show = [c for c in want if c in cols]
if ts is None or "rnti" not in cols or not show:
    sys.exit(
        f"columns not as expected (tstamp: {ts}, rnti: {'rnti' in cols}, values: {show})"
    )

key = [c for c in (node, "rnti") if c]
q = (
    f'SELECT {", ".join(key)}, CAST(({ts} - (SELECT MIN({ts}) FROM "{mac}")) / 5000000 AS INT) AS t5, '
    + ", ".join(f"AVG({c})" for c in show)
    + f' FROM "{mac}" GROUP BY {", ".join(key)}, t5 ORDER BY {", ".join(key)}, t5'
)
print("\n" + " ".join(f"{c:>9s}" for c in key + ["t[s]"] + show))
for r in con.execute(q):
    k, t5, vals = r[: len(key)], r[len(key)], r[len(key) + 1 :]
    k = [
        f"{v:x}" if c == "rnti" and isinstance(v, int) else str(v)
        for c, v in zip(key, k)
    ]
    print(
        " ".join(f"{v:>9s}" for v in k),
        f"{t5 * 5:>9d}",
        " ".join(f"{v:9.3f}" if v is not None else f"{'-':>9s}" for v in vals),
    )
