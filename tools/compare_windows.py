"""Compare Pulse window CSV exports without running GD. Exit 1 on any difference.

Other analyzers can be compared after adapting their CSV to the documented
columns and explicitly agreeing on scope, tick rate and width convention.
"""
import argparse
import csv
from pathlib import Path

IDENTITY = ("source_event_index", "tick", "player", "action")
FIELDS = ("gamemode", "active", "ignored", "measured", "negative_boundary",
          "positive_boundary", "earliest_passing_offset", "latest_passing_offset",
          "width_positions", "span_ticks", "early_censored", "late_censored",
          "negative_failure_tick", "positive_failure_tick", "success_endpoint_tick",
          "settling_ticks", "scope", "final_validation_warning", "uncertainty")


def read(path):
    with Path(path).open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        missing = set(IDENTITY + FIELDS) - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing columns {', '.join(sorted(missing))}")
        rows = {}
        for row in reader:
            key = tuple(row[name] for name in IDENTITY)
            if key in rows:
                raise ValueError(f"{path}: duplicate event identity {key}")
            rows[key] = row
        return rows


def differences(a, b):
    found = []
    for key in sorted(a.keys() | b.keys()):
        if key not in a or key not in b:
            found.append((*key, "event_presence", str(key in a), str(key in b)))
            continue
        for field in FIELDS:
            if a[key][field] != b[key][field]:
                found.append((*key, field, a[key][field], b[key][field]))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("accelerated", type=Path)
    parser.add_argument("full_restart", type=Path)
    parser.add_argument("--output", type=Path, default=Path("window-diff.csv"))
    args = parser.parse_args()
    try:
        a, b = read(args.accelerated), read(args.full_restart)
        found = differences(a, b)
        if args.output.resolve() in (args.accelerated.resolve(), args.full_restart.resolve()):
            raise ValueError("The output must not overwrite an input CSV")
        with args.output.open("w", newline="", encoding="utf-8") as out:
            writer = csv.writer(out)
            writer.writerow((*IDENTITY, "field", "accelerated", "full_restart"))
            writer.writerows(found)
        warned = any(row[name] == "1" for rows in (a, b) for row in rows.values()
                     for name in ("uncertainty", "final_validation_warning"))
        print(f"{len(found)} differences across {len(a)} / {len(b)} event rows; saved {args.output}.")
        if warned:
            print("WARNING: input results are unverified; matching widths do not establish equivalence.")
        return 1 if found or warned else 0
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
