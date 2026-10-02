"""Bundle the repository-ready source tree and write release SHA-256 hashes."""

import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
VERSION = json.loads((ROOT / "app" / "package.json").read_text(encoding="utf-8"))["version"]
EXE = DIST / f"BooleanFunctionLab-{VERSION}-portable.exe"
SOURCE = DIST / f"BooleanFunctionLab-{VERSION}-source.zip"
FILES = [
    ".gitignore",
    "README.md",
    "examples/README.md",
    "examples/single-output/quadratic_4var_truth_binary.txt",
    "examples/single-output/quadratic_4var_truth_hex.txt",
    "examples/single-output/quadratic_4var_anf.txt",
    "examples/multi-output/PRESENT_4x4_truth_hex.txt",
    "examples/multi-output/PRESENT_4x4_truth_binary.txt",
    "examples/multi-output/PRESENT_4x4_transposed_hex.txt",
    "examples/multi-output/PRESENT_4x4_transposed_binary.txt",
    "examples/multi-output/PRESENT_4x4_ANF.txt",
    "output/pdf/单输出布尔函数使用说明-v2.1.0.pdf",
    "output/pdf/多输出布尔函数使用说明-v2.1.0.pdf",
    "C++功能实现文件大纲.md",
    "单输出布尔函数安全性指标.xmind",
    "build_xmind.py",
    "cpp/bf_core.cpp",
    "cpp/build.ps1",
    "app/package.json",
    "app/pnpm-lock.yaml",
    "app/pnpm-workspace.yaml",
    "app/index.html",
    "app/vite.config.js",
    "app/electron/main.cjs",
    "app/electron/preload.cjs",
    "app/src/main.jsx",
    "app/src/MultiOutput.jsx",
    "app/src/styles.css",
    "app/src/multi-output.css",
    "app/build/icon.ico",
    "app/build/icon.png",
    "tests/test_core.py",
    "tests/fixtures/anf_user_style.txt",
    "scripts/create_icon.py",
    "scripts/create_user_guides.py",
    "scripts/build_multi_output_xmind.py",
    "scripts/make_release.py",
    "multi-output/.gitignore",
    "multi-output/README.md",
    "multi-output/cpp/vbf_core.cpp",
    "multi-output/cpp/build.ps1",
    "multi-output/tests/test_core.py",
    "multi-output/tests/fixtures/present.txt",
    "多输出布尔函数_C++功能实现文件大纲.md",
    "多输出布尔函数安全性指标.xmind",
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


if not EXE.is_file():
    raise SystemExit(f"Portable EXE missing: {EXE}")
for relative in FILES:
    if not (ROOT / relative).is_file():
        raise SystemExit(f"Source file missing: {relative}")
DIST.mkdir(exist_ok=True)
with ZipFile(SOURCE, "w", compression=ZIP_DEFLATED, compresslevel=9) as archive:
    for relative in FILES:
        archive.write(ROOT / relative, relative)
with ZipFile(SOURCE) as archive:
    if archive.testzip() is not None or sorted(archive.namelist()) != sorted(FILES):
        raise SystemExit("Source ZIP verification failed")
checksums = DIST / "SHA256SUMS.txt"
checksums.write_text(
    f"{sha256(EXE)}  {EXE.name}\n{sha256(SOURCE)}  {SOURCE.name}\n",
    encoding="utf-8",
)
print(f"Created {SOURCE.name} ({SOURCE.stat().st_size} bytes, {len(FILES)} files)")
print(f"Created {checksums.name}")
