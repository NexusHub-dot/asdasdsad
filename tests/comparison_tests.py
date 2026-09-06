import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("compare", Path(__file__).parents[1] / "tools/compare_windows.py")
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.row = dict.fromkeys(compare.IDENTITY + compare.FIELDS, "0")
        self.row.update(source_event_index="3", tick="100", player="1", action="press",
                        width_positions="6", earliest_passing_offset="-3", latest_passing_offset="2")
        self.key = tuple(self.row[k] for k in compare.IDENTITY)

    def test_identical(self):
        self.assertEqual(compare.differences({self.key: self.row}, {self.key: self.row}), [])

    def test_same_width_different_boundaries(self):
        changed = dict(self.row, earliest_passing_offset="-2", latest_passing_offset="3")
        self.assertEqual(len(compare.differences({self.key: self.row}, {self.key: changed})), 2)

    def test_missing_event(self):
        self.assertEqual(compare.differences({self.key: self.row}, {})[0][-3], "event_presence")

    def test_different_endpoint_is_not_comparable(self):
        changed = dict(self.row, success_endpoint_tick="200")
        self.assertEqual(compare.differences({self.key: self.row}, {self.key: changed})[0][-3], "success_endpoint_tick")

    def test_file_reader_rejects_duplicate_or_wrong_schema(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "windows.csv"
            with path.open("w", newline="") as out:
                writer = csv.DictWriter(out, fieldnames=compare.IDENTITY + compare.FIELDS)
                writer.writeheader()
                writer.writerow(self.row)
            self.assertEqual(compare.read(path)[self.key], self.row)
            with path.open("a", newline="") as out:
                csv.DictWriter(out, fieldnames=compare.IDENTITY + compare.FIELDS).writerow(self.row)
            with self.assertRaises(ValueError):
                compare.read(path)
            path.write_text("wrong,columns\n1,2\n")
            with self.assertRaises(ValueError):
                compare.read(path)


if __name__ == "__main__":
    unittest.main()
