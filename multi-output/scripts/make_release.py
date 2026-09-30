"""Create a source release ZIP and SHA-256 manifest after both review passes."""

import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[1]
PARENT = ROOT.parent
DIST = ROOT / "dist"
VERSION = json.loads((ROOT / "app" / "package.json").read_text(encoding="utf-8"))["version"]
EXE = DIST / f"VectorialBooleanLab-{VERSION}-portable.exe"
SOURCE = DIST / f"VectorialBooleanLab-{VERSION}-source.zip"
FILES = [
    ".gitignore", "README.md",
    "cpp/vbf_core.cpp", "cpp/build.ps1",
    "app/package.json", "app/pnpm-lock.yaml", "app/pnpm-workspace.yaml",
    "app/index.html", "app/vite.config.js",
    "app/electron/main.cjs", "app/electron/preload.cjs",
    "app/src/main.jsx", "app/src/styles.css",
    "app/build/icon.ico", "app/build/icon.png",
    "tests/test_core.py", "tests/fixtures/present.txt",
    "examples/PRESENT_4x4_truth_hex.txt", "examples/PRESENT_4x4_truth_binary.txt",
    "examples/PRESENT_4x4_transposed_hex.txt", "examples/PRESENT_4x4_transposed_binary.txt",
    "examples/PRESENT_4x4_ANF_reference.txt",
    "scripts/make_release.py",
]
DOCUMENTS = ["多输出布尔函数安全性指标.xmind", "多输出布尔函数_C++功能实现文件大纲.md"]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


if not EXE.is_file():
    raise SystemExit(f"Portable EXE missing: {EXE}")
for relative in FILES:
    if not (ROOT / relative).is_file():
        raise SystemExit(f"Source file missing: {relative}")
for name in DOCUMENTS:
    if not (PARENT / name).is_file():
        raise SystemExit(f"Document missing: {name}")
DIST.mkdir(exist_ok=True)
with ZipFile(SOURCE, "w", ZIP_DEFLATED, compresslevel=9) as archive:
    for relative in FILES:
        archive.write(ROOT / relative, relative)
    for name in DOCUMENTS:
        archive.write(PARENT / name, name)
with ZipFile(SOURCE) as archive:
    expected = sorted(FILES + DOCUMENTS)
    if archive.testzip() is not None or sorted(archive.namelist()) != expected:
        raise SystemExit("Source ZIP verification failed")
manifest = DIST / "SHA256SUMS.txt"
manifest.write_text(f"{sha256(EXE)}  {EXE.name}\n{sha256(SOURCE)}  {SOURCE.name}\n", encoding="utf-8")
print(f"Created {SOURCE.name}: {SOURCE.stat().st_size} bytes, {len(FILES) + len(DOCUMENTS)} files")
print(f"Created {manifest.name}")
