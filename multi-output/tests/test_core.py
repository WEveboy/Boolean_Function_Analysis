"""Independent mathematical checks for the multi-output C++ engine."""

import csv
import json
import math
import random
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "cpp" / "build" / "vbf-core.exe"
PRESENT = [12, 5, 6, 11, 9, 0, 10, 13, 3, 14, 15, 8, 4, 7, 1, 2]


def parity(value):
    return value.bit_count() & 1


def reference_ddt(values, m):
    n = len(values)
    return [[sum((values[x] ^ values[x ^ a]) == b for x in range(n))
             for b in range(1 << m)] for a in range(n)]


def reference_lat(values, m):
    n = len(values)
    return [[sum(1 - 2 * (parity(a & x) ^ parity(b & values[x])) for x in range(n))
             for b in range(1 << m)] for a in range(n)]


def reference_degree(truth):
    coefficients = list(truth)
    bit = 1
    while bit < len(truth):
        for mask in range(len(truth)):
            if mask & bit:
                coefficients[mask] ^= coefficients[mask ^ bit]
        bit <<= 1
    return max((mask.bit_count() for mask, coefficient in enumerate(coefficients) if coefficient), default=-1)


def reference_ai(truth):
    n = (len(truth) - 1).bit_length()
    for bound in range((n + 1) // 2 + 1):
        for candidate in range(1, 1 << len(truth)):
            g = [(candidate >> x) & 1 for x in range(len(truth))]
            if reference_degree(g) > bound:
                continue
            if all(not (f & v) for f, v in zip(truth, g)) or all(not ((1 ^ f) & v) for f, v in zip(truth, g)):
                return bound
    raise AssertionError("No annihilator found")


class CoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="vbf_test_")
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def run_core(self, values, n, m, metrics=(), ddt=False, lat=False, radix="hex", transpose=False):
        source = self.folder / "function.txt"
        if transpose:
            rows = ["".join(str((value >> (m - 1 - bit)) & 1) for value in values) for bit in range(m)]
            if radix == "hex":
                rows = [f"{int(row, 2):0{(len(values) + 3) // 4}X}" for row in rows]
            source.write_text("\n".join(rows), encoding="utf-8")
        elif radix == "hex":
            width = (m + 3) // 4
            source.write_text(" ".join(f"{v:0{width}X}" for v in values), encoding="utf-8")
        else:
            source.write_text("\n".join(f"{v:0{m}b}" for v in values), encoding="utf-8")
        out = self.folder / "results"
        args = [str(CORE), "--input", str(source), "--n", str(n), "--m", str(m),
                "--radix", radix, "--out", str(out), "--metrics", ",".join(metrics)]
        if ddt:
            args.append("--ddt")
        if lat:
            args.append("--lat")
        if transpose:
            args.append("--transpose")
        done = subprocess.run(args, capture_output=True, encoding="utf-8", timeout=120)
        self.assertTrue(done.stdout, done.stderr)
        data = json.loads(done.stdout)
        self.assertEqual(done.returncode == 0, data["ok"], data.get("error", done.stderr))
        return data

    def table(self, data, name):
        path = next(Path(p) for p in data["files"] if Path(p).name == name)
        with path.open(encoding="utf-8", newline="") as stream:
            rows = list(csv.reader(stream))
        return [[int(value) for value in row[1:]] for row in rows[1:]]

    def results(self, data):
        return {item["id"]: item for item in data["results"]}

    def test_present_tables_and_indicators(self):
        metrics = ["balance", "max_degree", "min_degree", "nonlinearity", "resiliency",
                   "diff_uniformity", "diff_probability", "diff_deviation", "diff_stddev",
                   "apn", "linearity", "linear_bias", "linear_probability", "ab"]
        data = self.run_core(PRESENT, 4, 4, metrics, ddt=True, lat=True)
        self.assertEqual(self.table(data, "DDT.csv"), reference_ddt(PRESENT, 4))
        self.assertEqual(self.table(data, "LAT.csv"), reference_lat(PRESENT, 4))
        self.assertEqual(len(data["files"]), 4)
        r = self.results(data)
        self.assertEqual(r["balance"]["value"], "满足")
        self.assertEqual(r["nonlinearity"]["value"], "4")
        self.assertEqual(r["diff_uniformity"]["value"], "4")
        self.assertEqual(r["diff_probability"]["value"], "0.250000")
        self.assertEqual(r["linearity"]["value"], "8")
        self.assertEqual(r["linear_bias"]["value"], "0.250000")
        self.assertEqual(r["linear_probability"]["value"], "0.750000")
        self.assertEqual(r["apn"]["value"], "不满足")
        self.assertEqual(r["ab"]["value"], "不满足")
        self.assertEqual(data["input"]["permutation"], True)
        truth = next(Path(p) for p in data["files"] if Path(p).name == "Truth_table.txt")
        self.assertIn("1100", truth.read_text(encoding="utf-8"))

    def test_identity_and_single_request_independence(self):
        values = list(range(8))
        data = self.run_core(values, 3, 3, ["linearity"], radix="bin")
        self.assertEqual(set(self.results(data)), {"linearity"})
        self.assertEqual(self.results(data)["linearity"]["value"], "8")
        self.assertEqual({Path(p).name for p in data["files"]}, {"Truth_table.txt", "ANF.txt"})
        data = self.run_core(values, 3, 3, ["diff_uniformity"], ddt=True)
        self.assertEqual(self.results(data)["diff_uniformity"]["value"], "8")
        self.assertEqual(self.table(data, "DDT.csv"), reference_ddt(values, 3))
        self.assertNotIn("LAT.csv", {Path(p).name for p in data["files"]})

    def test_random_small_tables_against_direct_formulas(self):
        generator = random.Random(20260929)
        for n, m in [(3, 2), (4, 3), (5, 3)]:
            values = [generator.randrange(1 << m) for _ in range(1 << n)]
            data = self.run_core(values, n, m, ["diff_uniformity", "linearity"], ddt=True, lat=True)
            ddt = reference_ddt(values, m)
            lat = reference_lat(values, m)
            self.assertEqual(self.table(data, "DDT.csv"), ddt)
            self.assertEqual(self.table(data, "LAT.csv"), lat)
            self.assertEqual(int(self.results(data)["diff_uniformity"]["value"]), max(v for row in ddt[1:] for v in row))
            self.assertEqual(int(self.results(data)["linearity"]["value"]), max(abs(v) for row in lat[1:] for v in row[1:]))

    def test_constant_degenerate_and_invalid_input(self):
        data = self.run_core([0] * 8, 3, 1, ["balance", "max_degree", "min_degree", "ai", "linearity", "linear_probability"])
        r = self.results(data)
        self.assertEqual(r["balance"]["value"], "不满足")
        self.assertEqual(r["max_degree"]["value"], "零多项式")
        self.assertEqual(r["min_degree"]["value"], "零多项式")
        self.assertEqual(r["ai"]["value"], "0")
        self.assertEqual(r["linearity"]["value"], "0")
        self.assertEqual(r["linear_probability"]["value"], "0.500000")
        bad = self.folder / "bad.txt"
        bad.write_text("C 5 6", encoding="utf-8")
        done = subprocess.run([str(CORE), "--input", str(bad), "--n", "4", "--m", "4"], capture_output=True, encoding="utf-8")
        self.assertNotEqual(done.returncode, 0)
        self.assertFalse(json.loads(done.stdout)["ok"])

    def test_independent_summary_formulas(self):
        generator = random.Random(23897)
        n, m = 3, 2
        values = [generator.randrange(1 << m) for _ in range(1 << n)]
        metrics = ["max_degree", "min_degree", "ai", "nonlinearity", "resiliency",
                   "diff_uniformity", "diff_probability", "diff_deviation", "diff_stddev",
                   "linearity", "linear_bias", "linear_probability"]
        data = self.run_core(values, n, m, metrics)
        r = self.results(data)
        components = [[parity(b & y) for y in values] for b in range(1, 1 << m)]
        degrees = [reference_degree(component) for component in components]
        self.assertEqual(int(r["max_degree"]["value"]), max(degrees))
        self.assertEqual(int(r["min_degree"]["value"]), min(degrees))
        self.assertEqual(int(r["ai"]["value"]), min(reference_ai(component) for component in components))
        lat = reference_lat(values, m)
        peak = max(abs(lat[a][b]) for a in range(1 << n) for b in range(1, 1 << m))
        linear_peak = max(abs(lat[a][b]) for a in range(1, 1 << n) for b in range(1, 1 << m))
        self.assertEqual(int(r["nonlinearity"]["value"]), (1 << (n - 1)) - peak // 2)
        self.assertEqual(int(r["linearity"]["value"]), linear_peak)
        self.assertEqual(float(r["linear_bias"]["value"]), round(linear_peak / (2 * (1 << n)), 6))
        self.assertEqual(float(r["linear_probability"]["value"]), round(.5 + linear_peak / (2 * (1 << n)), 6))
        first = min((a.bit_count() for a in range(1 << n) for b in range(1, 1 << m) if lat[a][b]), default=n + 1)
        expected_order = min(n - m, first - 1) if first else -1
        self.assertEqual(r["resiliency"]["value"], "不具备0阶弹性" if expected_order < 0 else str(expected_order))
        ddt = reference_ddt(values, m)
        cells = [v for row in ddt[1:] for v in row]
        delta = max(cells)
        ideal = (1 << n) // (1 << m)
        self.assertEqual(int(r["diff_uniformity"]["value"]), delta)
        self.assertEqual(float(r["diff_probability"]["value"]), round(delta / (1 << n), 6))
        self.assertEqual(int(r["diff_deviation"]["value"]), max(abs(v - ideal) for v in cells))
        self.assertAlmostEqual(float(r["diff_stddev"]["value"]), math.sqrt(sum((v - ideal) ** 2 for v in cells) / len(cells)), places=5)

    def test_contiguous_hex_and_table_limit(self):
        source = self.folder / "continuous.txt"
        source.write_text("C56B90AD3EF84712", encoding="utf-8")
        done = subprocess.run([str(CORE), "--input", str(source), "--n", "4", "--m", "4", "--radix", "hex", "--metrics", "diff_uniformity"], capture_output=True, encoding="utf-8")
        self.assertEqual(done.returncode, 0, done.stdout)
        self.assertEqual(self.results(json.loads(done.stdout))["diff_uniformity"]["value"], "4")
        big = self.folder / "large.txt"
        big.write_text(" ".join("00" for _ in range(4096)), encoding="utf-8")
        out = self.folder / "limited"
        done = subprocess.run([str(CORE), "--input", str(big), "--n", "12", "--m", "8", "--radix", "hex", "--metrics", "", "--lat", "--out", str(out)], capture_output=True, encoding="utf-8")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("131072", json.loads(done.stdout)["error"])
        self.assertFalse(out.exists())

    def test_transposed_formats_match_standard_outputs(self):
        metrics = ["balance", "max_degree", "nonlinearity", "diff_uniformity", "linearity"]
        standard = self.run_core(PRESENT, 4, 4, metrics, ddt=True, lat=True)
        self.assertFalse(standard["input"]["transposed"])
        expected = {Path(path).name: Path(path).read_bytes() for path in standard["files"]}
        for radix in ("bin", "hex"):
            with self.subTest(radix=radix):
                data = self.run_core(PRESENT, 4, 4, metrics, ddt=True, lat=True, radix=radix, transpose=True)
                self.assertTrue(data["input"]["transposed"])
                self.assertEqual(self.results(data), self.results(standard))
                self.assertEqual({Path(path).name: Path(path).read_bytes() for path in data["files"]}, expected)

    def test_transposed_fixture_and_padding(self):
        for name, radix in (("PRESENT_4x4_transposed_binary.txt", "bin"),
                            ("PRESENT_4x4_transposed_hex.txt", "hex")):
            with self.subTest(name=name):
                source = ROOT / "examples" / name
                done = subprocess.run([str(CORE), "--input", str(source), "--n", "4", "--m", "4",
                                       "--radix", radix, "--transpose", "--metrics", "balance",
                                       "--out", str(self.folder / "fixture_results")],
                                      capture_output=True, encoding="utf-8")
                self.assertEqual(done.returncode, 0, done.stdout)
                self.assertEqual(self.results(json.loads(done.stdout))["balance"]["value"], "满足")
        data = self.run_core([0, 1], 1, 1, ["balance"], radix="hex", transpose=True)
        self.assertTrue(data["input"]["transposed"])

    def test_transposed_invalid_rows(self):
        source = self.folder / "bad_transposed.txt"
        args = [str(CORE), "--input", str(source), "--n", "4", "--m", "4", "--radix", "hex", "--transpose"]
        for content, expected in (("9B70\nE16C\n32E5", "4 个非空坐标行"),
                                  ("9B70\nE16C\n32E5\n59A", "4 个十六进制位"),
                                  ("9B70\nE16C\n32E5\n59AG", "非法十六进制字符")):
            with self.subTest(content=content):
                source.write_text(content, encoding="utf-8")
                done = subprocess.run(args, capture_output=True, encoding="utf-8")
                self.assertNotEqual(done.returncode, 0)
                self.assertIn(expected, json.loads(done.stdout)["error"])
        source.write_text("C", encoding="utf-8")
        done = subprocess.run([str(CORE), "--input", str(source), "--n", "1", "--m", "1",
                               "--radix", "hex", "--transpose"], capture_output=True, encoding="utf-8")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("高位填充", json.loads(done.stdout)["error"])


if __name__ == "__main__":
    unittest.main()
