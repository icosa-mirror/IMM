#!/usr/bin/env python3
"""Give a bundled iOS JPEG implementation private symbols before host linking."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def llvm_tool(name: str, override: str | None) -> str:
    if override:
        return override
    found = shutil.which(name)
    if found:
        return found
    for root in (Path("/opt/homebrew/opt/llvm/bin"), Path("/usr/local/opt/llvm/bin")):
        candidate = root / name
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError(f"{name} is required to isolate iOS JPEG symbols; install LLVM (brew install llvm)")


def symbols(nm: str, archive: Path, defined: bool) -> set[str]:
    command = [nm, "--extern-only", "--just-symbol-name"]
    if defined:
        command.append("--defined-only")
    output = subprocess.check_output([*command, str(archive)], text=True)
    result = set()
    for line in output.splitlines():
        name = line.strip()
        if not name or name.endswith(":"):
            continue
        # Objective-C block descriptors can contain punctuation/control bytes.
        # Preserve those names verbatim; only codec names enter the rename map.
        result.add(name)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--jpeg-archive", required=True, type=Path)
    parser.add_argument("--nm")
    parser.add_argument("--objcopy")
    args = parser.parse_args()
    nm = llvm_tool("llvm-nm", args.nm)
    objcopy = llvm_tool("llvm-objcopy", args.objcopy)
    jpeg = symbols(nm, args.jpeg_archive, defined=True)
    if not {"_jpeg_CreateDecompress", "_jpeg_std_error"} <= jpeg or "_UnityPluginLoad" in jpeg:
        raise RuntimeError("Expected the Mach-O JPEG dependency archive, not the combined Unity plugin")
    if any(not re.fullmatch(r"_[A-Za-z0-9_.$]+", name) for name in jpeg):
        raise RuntimeError("Expected Mach-O C codec symbols with leading underscores")
    before = symbols(nm, args.archive, defined=True)
    if not jpeg <= before:
        raise RuntimeError("The combined archive does not contain the entire JPEG dependency")
    renamed = {name: f"_imm_unity{name}" for name in jpeg}
    if set(renamed.values()) & before:
        raise RuntimeError("The private JPEG namespace is already present")
    with tempfile.TemporaryDirectory(prefix="imm-jpeg-", dir=args.archive.parent) as temporary:
        mapping = Path(temporary) / "symbols.txt"
        mapping.write_text("".join(f"{name} {renamed[name]}\n" for name in sorted(jpeg)), encoding="utf-8")
        output = Path(temporary) / args.archive.name
        subprocess.run([objcopy, f"--redefine-syms={mapping}", str(args.archive), str(output)], check=True)
        after = symbols(nm, output, defined=True)
        if after != (before - jpeg) | set(renamed.values()):
            raise RuntimeError("JPEG isolation changed unexpected exported symbols")
        if symbols(nm, output, defined=False) & jpeg:
            raise RuntimeError("Unisolated JPEG definitions or references remain")
        # Replace only after verifying both private symbols and the untouched plugin API.
        output.replace(args.archive)
    print(f"Isolated {len(jpeg)} iOS JPEG symbols; other plugin symbols preserved")


if __name__ == "__main__":
    main()
