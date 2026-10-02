#!/usr/bin/env python3
"""Give bundled iOS codecs private symbols before linking into Unity's framework."""

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
    raise RuntimeError(f"{name} is required to isolate iOS codec symbols; install LLVM (brew install llvm)")


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
    parser.add_argument("--codec-archive", required=True, action="append", type=Path)
    parser.add_argument("--nm")
    parser.add_argument("--objcopy")
    args = parser.parse_args()
    nm = llvm_tool("llvm-nm", args.nm)
    objcopy = llvm_tool("llvm-objcopy", args.objcopy)
    codecs = set()
    for archive in args.codec_archive:
        exported = symbols(nm, archive, defined=True)
        if not exported or "_UnityPluginLoad" in exported:
            raise RuntimeError("Expected a codec dependency archive, not the combined Unity plugin")
        codecs.update(exported)
    if any(not re.fullmatch(r"_[A-Za-z0-9_.$]+", name) for name in codecs):
        raise RuntimeError("Expected Mach-O C codec symbols with leading underscores")
    before = symbols(nm, args.archive, defined=True)
    if not codecs <= before:
        raise RuntimeError("The combined archive does not contain the entire codec dependencies")
    renamed = {name: f"_imm_unity{name}" for name in codecs}
    if set(renamed.values()) & before:
        raise RuntimeError("The private codec namespace is already present")
    with tempfile.TemporaryDirectory(prefix="imm-codecs-", dir=args.archive.parent) as temporary:
        mapping = Path(temporary) / "symbols.txt"
        mapping.write_text("".join(f"{name} {renamed[name]}\n" for name in sorted(codecs)), encoding="utf-8")
        output = Path(temporary) / args.archive.name
        subprocess.run([objcopy, f"--redefine-syms={mapping}", str(args.archive), str(output)], check=True)
        after = symbols(nm, output, defined=True)
        if after != (before - codecs) | set(renamed.values()):
            raise RuntimeError("Codec isolation changed unexpected exported symbols")
        if symbols(nm, output, defined=False) & codecs:
            raise RuntimeError("Unisolated codec definitions or references remain")
        # Replace only after verifying both private symbols and the untouched plugin API.
        output.replace(args.archive)
    print(f"Isolated {len(codecs)} iOS codec symbols; other plugin symbols preserved")


if __name__ == "__main__":
    main()
