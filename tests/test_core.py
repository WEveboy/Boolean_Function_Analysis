"""Independent reference checks for the C++ command-line engine."""

import json
import random
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "cpp" / "build" / "bf-core.exe"


def parity(value):
    return value.bit_count() & 1


def anf_coefficients(truth):
    values = list(truth)
    bit = 1
    while bit < len(values):
        for mask in range(len(values)):
            if mask & bit:
                values[mask] ^= values[mask ^ bit]
        bit <<= 1
    return values


def degree(truth):
    return max((mask.bit_count() for mask, coefficient in enumerate(anf_coefficients(truth)) if coefficient), default=-1)


def walsh(truth):
    size = len(truth)
    return [sum((1 if not value ^ parity(index & mask) else -1) for index, value in enumerate(truth)) for mask in range(size)]


def correlation(f, g):
    return [sum(1 if f[x] == g[x ^ mask] else -1 for x in range(len(f))) for mask in range(len(f))]


def fai_reference(truth):
    n = (len(truth) - 1).bit_length()
    monomials = [m for m in range(len(truth)) if 2 * m.bit_count() < n]
    best = None
    for selected in range(1, 1 << len(monomials)):
        g = [0] * len(truth)
        for bit, monomial in enumerate(monomials):
            if selected & (1 << bit):
                for x in range(len(truth)):
                    g[x] ^= int(x & monomial == monomial)
        product = [a & b for a, b in zip(truth, g)]
        if any(product):
            value = degree(g) + degree(product)
            best = value if best is None else min(best, value)
    return best


def ai_reference(truth):
    size = len(truth)
    for bound in range((size.bit_length()) // 2 + 1):
        for coefficient_bits in range(1, 1 << size):
            monomials = [m for m in range(size) if coefficient_bits & (1 << m)]
            if max(m.bit_count() for m in monomials) > bound:
                continue
            values = [parity(sum(1 << j for j, m in enumerate(monomials) if x & m == m)) for x in range(size)]
            if all(not (f & g) for f, g in zip(truth, values)) or all(not ((1 ^ f) & g) for f, g in zip(truth, values)):
                return bound
    raise AssertionError("AI candidate not found")


class CoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bf_core_")
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def run_core(self, content, metrics="", more=(), second=None, suffix="f.txt"):
        source = self.root / suffix
        source.write_text(content, encoding="utf-8")
        out = self.root / "output"
        args = [str(CORE), "--input", str(source), "--out", str(out), "--metrics", metrics, *more]
        if second is not None:
            second_path = self.root / "g.txt"
            second_path.write_text(second, encoding="utf-8")
            args += ["--input2", str(second_path)]
        done = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=90)
        data = json.loads(done.stdout)
        self.assertEqual(done.returncode == 0, data["ok"], done.stderr)
        return data

    def test_binary_hex_anf_and_outputs(self):
        binary = self.run_core(" 0 0\n0 1\t", "balance,degree,nonlinearity")
        self.assertTrue(binary["ok"])
        self.assertEqual(binary["inputs"][0]["radix"], "bin")
        generated = {Path(p).name: Path(p).read_text(encoding="utf-8") for p in binary["files"]}
        self.assertEqual(set(generated), {"Truth_table.txt", "ANF.txt"})
        self.assertEqual(generated["Truth_table.txt"].strip(), "0001")
        self.assertIn("x1*x2", generated["ANF.txt"])
        self.assertEqual([r["id"] for r in binary["results"]], ["balance", "degree", "nonlinearity"])

        hexadecimal = self.run_core("0x 0 1", "", suffix="h.TXT")
        self.assertEqual(hexadecimal["inputs"][0]["radix"], "hex")
        self.assertEqual(Path(hexadecimal["files"][0]).read_text().strip(), "00000001")
        anf = self.run_core("n = 2; x1 * x2", "", suffix="a.txt")
        self.assertEqual(Path(anf["files"][0]).read_text().strip(), "0001")
        self.assertEqual(anf["results"], [])
        self.assertEqual(len(anf["files"]), 2)

    def test_shipped_single_output_anf_example(self):
        source = ROOT / "examples" / "single-output" / "quadratic_4var_anf.txt"
        out = self.root / "shipped-anf-output"
        done = subprocess.run(
            [str(CORE), "--input", str(source), "--kind", "auto", "--radix", "auto",
             "--metrics", "balance,degree,nonlinearity", "--out", str(out)],
            capture_output=True, text=True, encoding="utf-8", timeout=90,
        )
        self.assertEqual(done.returncode, 0, done.stdout)
        data = json.loads(done.stdout)
        self.assertTrue(data["ok"], data.get("error"))
        self.assertEqual(data["inputs"][0]["kind"], "anf")
        self.assertEqual((out / "Truth_table.txt").read_text(encoding="utf-8").strip(), "1100110011000011")

    def test_user_anf_style_and_fixed_output_names(self):
        example = (ROOT / "tests" / "fixtures" / "anf_user_style.txt").read_text(encoding="utf-8")
        data = self.run_core(example, "balance,degree", suffix="示例.txt")
        self.assertTrue(data["ok"], data.get("error"))
        self.assertEqual(data["inputs"][0]["n"], 4)
        self.assertEqual(data["inputs"][0]["kind"], "anf")
        self.assertEqual([Path(path).name for path in data["files"]], ["Truth_table.txt", "ANF.txt"])
        values = []
        for x in range(16):
            x1, x2, x3, x4 = ((x >> bit) & 1 for bit in (3, 2, 1, 0))
            values.append(x4 * x3 ^ x3 * x2 ^ x2 * x1 ^ x4 * x1 ^ x3)
        self.assertEqual(Path(data["files"][0]).read_text().strip(), "".join(map(str, values)))
        self.assertIn("x1*x4", Path(data["files"][1]).read_text())
        again = self.run_core(example, suffix="示例.txt")
        self.assertTrue(again["ok"])
        self.assertEqual([Path(path).name for path in again["files"]], ["Truth_table.txt", "ANF.txt"])
        self.assertNotEqual(Path(data["files"][0]).parent, Path(again["files"][0]).parent)
        self.assertEqual(len(list(Path(again["files"][0]).parent.iterdir())), 2)

    def test_nonbreaking_spaces_in_truth_and_anf(self):
        spaces = "\u00a0\u202f\u2007\u200b\ufeff"
        truth = self.run_core(f"0{spaces[0]}0{spaces[1]}0{spaces[2]}1", suffix="spaces.txt")
        self.assertTrue(truth["ok"], truth.get("error"))
        self.assertEqual(Path(truth["files"][0]).read_text().strip(), "0001")
        anf = self.run_core(f"f(x2{spaces[0]},x1)=x2{spaces[1]}x1{spaces[3]}{spaces[4]}", suffix="spaces-anf.txt")
        self.assertTrue(anf["ok"], anf.get("error"))
        self.assertEqual(anf["inputs"][0]["kind"], "anf")
        self.assertEqual([Path(p).name for p in anf["files"]], ["Truth_table.txt", "ANF.txt"])
        self.assertEqual(Path(anf["files"][0]).read_text().strip(), "0001")

    def test_random_spectra_and_fai(self):
        random.seed(34)
        for n in (2, 3, 4):
            for index in range(6):
                f = [random.randrange(2) for _ in range(1 << n)]
                g = [random.randrange(2) for _ in range(1 << n)]
                name = f"f{n}_{index}.txt"
                data = self.run_core("".join(map(str, f)), "degree,nonlinearity,walsh_dist,autocorr_dist,fai", suffix=name)
                self.assertTrue(data["ok"], data.get("error"))
                items = {r["id"]: r for r in data["results"]}
                w = walsh(f)
                c = correlation(f, f)
                self.assertEqual(int(items["degree"]["value"]) if items["degree"]["value"] != "未定义" else -1, degree(f))
                self.assertEqual(int(items["nonlinearity"]["value"]), len(f) // 2 - max(map(abs, w)) // 2)
                for metric, expected in (("walsh_dist", w), ("autocorr_dist", c)):
                    counts = {v: expected.count(v) for v in set(expected)}
                    self.assertEqual({row["value"]: row["count"] for row in items[metric]["counts"]}, counts)
                    self.assertEqual(items[metric]["stats"]["minAbs"], min(map(abs, expected)))
                    self.assertEqual(items[metric]["stats"]["maxAbs"], max(map(abs, expected)))
                expected_fai = fai_reference(f)
                self.assertEqual(items["fai"]["value"], str(expected_fai) if expected_fai is not None else "不适用")
                cross = self.run_core("".join(map(str, f)), "crosscorr_dist", second="".join(map(str, g)), suffix=name)
                self.assertTrue(cross["ok"], cross.get("error"))
                values = correlation(f, g)
                self.assertEqual({r["value"]: r["count"] for r in cross["results"][0]["counts"]}, {v: values.count(v) for v in set(values)})

    def test_ea_and_invalid_inputs(self):
        data = self.run_core("0001", "ea,degree,nonlinearity", more=("--ea-rows", "01,10", "--ea-alpha", "01", "--ea-beta", "10", "--ea-epsilon", "1"))
        self.assertTrue(data["ok"])
        self.assertEqual([Path(p).name for p in data["files"]], ["Truth_table.txt", "ANF.txt"])
        original = [0, 0, 0, 1]
        expected = [original[(((x & 2) >> 1) | ((x & 1) << 1)) ^ 1] ^ ((x & 2) >> 1) ^ 1 for x in range(4)]
        self.assertEqual(Path(data["files"][0]).read_text().strip(), "0001")
        self.assertEqual([r["id"] for r in data["transformedResults"]], ["degree", "nonlinearity"])
        self.assertEqual(data["transformedResults"][0]["value"], str(degree(expected)))
        self.assertEqual(data["transformedResults"][1]["value"], str(len(expected) // 2 - max(map(abs, walsh(expected))) // 2))
        bad = self.run_core("0x01", "crosscorr_dist", second="00000000")
        self.assertFalse(bad["ok"])
        self.assertIn("进制不一致", bad["error"])
        bad_matrix = self.run_core("0001", "ea", more=("--ea-rows", "10,10"), suffix="invalid.txt")
        self.assertFalse(bad_matrix["ok"])
        self.assertEqual([Path(p).name for p in data["files"]], ["Truth_table.txt", "ANF.txt"])

    def test_ai_diffusion_and_correlation(self):
        random.seed(91)
        for index in range(6):
            f = [random.randrange(2) for _ in range(8)]
            data = self.run_core("".join(map(str, f)), "ai,sac,pc,gac,correlation_immunity,resiliency,balance", more=("--pc-k", "2"), suffix=f"v{index}.txt")
            self.assertTrue(data["ok"], data.get("error"))
            items = {r["id"]: r for r in data["results"]}
            self.assertEqual(int(items["ai"]["value"]), ai_reference(f))
            c = correlation(f, f)
            self.assertEqual(items["sac"]["value"] == "满足", all(c[1 << bit] == 0 for bit in range(3)))
            self.assertEqual(items["pc"]["value"] == "满足", all(c[mask] == 0 for mask in range(1, 8) if mask.bit_count() <= 2))
            self.assertEqual(items["gac"]["value"], f"Δ={max(map(abs, c[1:]))}")
            self.assertIn(f"σ={sum(value * value for value in c)}", items["gac"]["detail"])
            w = walsh(f)
            order = next((mask.bit_count() - 1 for mask in sorted(range(1, 8), key=lambda x: x.bit_count()) if w[mask]), 3)
            self.assertEqual(int(items["correlation_immunity"]["value"]), order)
            self.assertEqual(items["resiliency"]["value"], str(order) if not w[0] else "不适用")
            self.assertEqual(items["balance"]["value"] == "满足", sum(f) == 4)


if __name__ == "__main__":
    unittest.main()
