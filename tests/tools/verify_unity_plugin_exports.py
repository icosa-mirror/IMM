#!/usr/bin/env python3
"""Verify that every C# P/Invoke entry point exists in the shipped IMM Unity plugin binary.

The Unity packages declare native entry points with [DllImport(...)] against two
plugin binaries:

    com.immersive-foundation.imm-unity           -> ImmUnityPlugin
    com.immersive-foundation.imm-stroke-reader   -> ImmStrokeReader

Nothing in the build currently checks that those entry points actually exist in
the plugin binary for the platform being shipped, so a C# declaration can drift
away from the native export list and only fail at runtime with
EntryPointNotFoundException. This tool performs that check offline:

    python tests/tools/verify_unity_plugin_exports.py
    python tests/tools/verify_unity_plugin_exports.py --platform android
    python tests/tools/verify_unity_plugin_exports.py --json artifacts/unity-plugin-exports.json

Exit code 0 means every declared entry point was found in the selected platform
binary; 1 means at least one was missing or a binary could not be read.

The PE/ELF/Mach-O readers are intentionally dependency free (no dumpbin, no
llvm-objdump) so the check also runs on Linux CI runners.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

PACKAGE_ROOT = "code/ImmUnitySampleProject/Packages"

# plugin name -> platform -> binary path relative to the repo root
PLUGIN_BINARIES = {
    "ImmUnityPlugin": {
        "windows": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll",
        "android": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so",
        "ios": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/iOS/libImmUnityPlugin.a",
        "macos": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX/ImmUnityPlugin.bundle/Contents/MacOS/ImmUnityPlugin",
    },
    "ImmStrokeReader": {
        "windows": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll",
        "android": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/Android/arm64-v8a/libImmStrokeReader.so",
        "ios": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/iOS/libImmStrokeReader.a",
        "macos": "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/macOS/libImmStrokeReader.dylib",
    },
}

# C# source directories scanned for DllImport declarations, per plugin.
CSHARP_SOURCES = {
    "ImmUnityPlugin": [
        "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime",
    ],
    "ImmStrokeReader": [
        "code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime",
    ],
}

DLLIMPORT_RE = re.compile(
    r"\[DllImport\(\s*(?P<args>[^\]]*?)\)\]\s*(?:\[[^\]]*\]\s*)*"
    r"(?:public|private|internal|protected)?\s*(?:static\s+)?(?:extern\s+)?"
    r"(?P<ret>[\w\.<>\[\]:\?]+)\s+(?P<name>\w+)\s*\(",
    re.DOTALL,
)
ENTRYPOINT_RE = re.compile(r'EntryPoint\s*=\s*"(?P<name>[^"]+)"')
PREPROCESSOR_RE = re.compile(r"^\s*#\s*(?P<directive>if|ifdef|ifndef|elif|else|endif)\b\s*(?P<rest>.*)$")

# Entry points that are intentionally absent from a shipped platform binary.
# Each entry is (plugin, platform) -> {name or prefix: reason}. Anything not
# listed here is treated as drift and fails the check.
#
# Empty today: the exporter is built for every platform (Windows through
# appImmUnity.vcxproj, Android/iOS/macOS through their CMake targets), so every
# declared entry point is expected in every shipped binary.
KNOWN_PLATFORM_GAPS = {}

# Symbols a platform provides outside the plugin binary itself.
PLATFORM_PROVIDED = {
    ("ImmUnityPlugin", "ios"): {
        "ImmUnityRegisterRenderingPlugin": "defined in Plugins/iOS/ImmUnityPluginRegister.mm",
    },
}

MH_MAGIC_64 = 0xFEEDFACF
MH_CIGAM_64 = 0xCFFAEDFE
MH_MAGIC = 0xFEEDFACE
MH_CIGAM = 0xCEFAEDFE
FAT_MAGIC = 0xCAFEBABE
FAT_MAGIC_64 = 0xCAFEBABF


def pe_exports(data: bytes) -> set[str]:
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    coff = e_lfanew + 4
    num_sections, = struct.unpack_from("<H", data, coff + 2)
    size_opt, = struct.unpack_from("<H", data, coff + 16)
    opt = coff + 20
    magic, = struct.unpack_from("<H", data, opt)
    dd = opt + (112 if magic == 0x20B else 96)
    exp_rva, _ = struct.unpack_from("<II", data, dd)

    sections = []
    sec = opt + size_opt
    for i in range(num_sections):
        off = sec + i * 40
        vsize, vaddr, raw_size, raw_ptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((vaddr, max(vsize, raw_size), raw_ptr))

    def rva2off(rva: int) -> int:
        for vaddr, size, raw_ptr in sections:
            if vaddr <= rva < vaddr + size:
                return raw_ptr + (rva - vaddr)
        raise ValueError(f"RVA {rva:#x} is not mapped")

    n_names, = struct.unpack_from("<I", data, rva2off(exp_rva) + 24)
    names_rva, = struct.unpack_from("<I", data, rva2off(exp_rva) + 32)
    names_off = rva2off(names_rva)
    out = set()
    for i in range(n_names):
        name_rva, = struct.unpack_from("<I", data, names_off + i * 4)
        off = rva2off(name_rva)
        out.add(data[off : data.index(b"\0", off)].decode("ascii", "replace"))
    return out


def elf_exports(data: bytes) -> set[str]:
    if data[:4] != b"\x7fELF":
        raise ValueError("not an ELF file")
    is64 = data[4] == 2
    endian = "<" if data[5] == 1 else ">"
    if is64:
        e_shoff, = struct.unpack_from(endian + "Q", data, 0x28)
        e_shentsize, e_shnum, _ = struct.unpack_from(endian + "HHH", data, 0x3A)
    else:
        e_shoff, = struct.unpack_from(endian + "I", data, 0x20)
        e_shentsize, e_shnum, _ = struct.unpack_from(endian + "HHH", data, 0x2E)

    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        if is64:
            name, typ, _f, _a, offset, size, link, _i, _al, entsize = struct.unpack_from(
                endian + "IIQQQQIIQQ", data, off
            )
        else:
            name, typ, _f, _a, offset, size, link, _i, _al, entsize = struct.unpack_from(
                endian + "IIIIIIIIII", data, off
            )
        sections.append({"name": name, "type": typ, "offset": offset, "size": size,
                         "link": link, "entsize": entsize})

    out = set()
    for section in sections:
        if section["type"] != 11:  # SHT_DYNSYM
            continue
        strtab = sections[section["link"]]
        ent = section["entsize"] or (24 if is64 else 16)
        for off in range(section["offset"], section["offset"] + section["size"], ent):
            if is64:
                st_name, st_info, _other, st_shndx = struct.unpack_from(endian + "IBBH", data, off)
            else:
                st_name, _val, _sz, st_info, _other, st_shndx = struct.unpack_from(
                    endian + "IIIIBB", data, off
                )
            if st_shndx == 0 or (st_info >> 4) not in (1, 2) or st_name == 0:
                continue
            start = strtab["offset"] + st_name
            out.add(data[start : data.index(b"\0", start)].decode("ascii", "replace"))
    return out


def _macho_slice_exports(data: bytes, base: int) -> set[str]:
    magic, = struct.unpack_from(">I", data, base)
    if magic in (MH_MAGIC_64, MH_CIGAM_64):
        is64, endian = True, ("<" if magic == MH_CIGAM_64 else ">")
    elif magic in (MH_MAGIC, MH_CIGAM):
        is64, endian = False, ("<" if magic == MH_CIGAM else ">")
    else:
        return set()

    ncmds, = struct.unpack_from(endian + "I", data, base + 16)
    off = base + (32 if is64 else 28)
    out: set[str] = set()
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from(endian + "II", data, off)
        if cmd == 0x2:  # LC_SYMTAB
            symoff, nsyms, stroff, _strsize = struct.unpack_from(endian + "IIII", data, off + 8)
            ent = 16 if is64 else 12
            for i in range(nsyms):
                p = symoff + i * ent
                n_strx, n_type, n_sect, _desc = struct.unpack_from(endian + "IBBH", data, p)
                # n_value may legitimately be 0 for a defined symbol in an object file.
                if not (n_type & 0x01) or (n_type & 0x0E) == 0 or n_sect == 0:
                    continue
                start = stroff + n_strx
                if data[start : start + 1] == b"\0":
                    continue
                out.add(data[start : data.index(b"\0", start)].decode("ascii", "replace"))
        off += cmdsize
    return out


def _ar_members(data: bytes):
    if data[:8] != b"!<arch>\n":
        raise ValueError("not an ar archive")
    off = 8
    while off + 60 <= len(data):
        header = data[off : off + 60]
        name = header[0:16].decode("ascii", "replace").strip()
        size = int(header[48:58].decode("ascii", "replace").strip() or "0")
        body = off + 60
        if name.startswith("#1/"):
            namelen = int(name[3:])
            yield data[body + namelen : body + size]
        else:
            yield data[body : body + size]
        off = body + size + (size % 2)


def macho_exports(data: bytes) -> set[str]:
    magic, = struct.unpack_from(">I", data, 0)
    if magic in (FAT_MAGIC, FAT_MAGIC_64):
        nfat, = struct.unpack_from(">I", data, 4)
        is64 = magic == FAT_MAGIC_64
        ent = 32 if is64 else 20
        out: set[str] = set()
        for i in range(nfat):
            off = 8 + i * ent
            if is64:
                offset, size = struct.unpack_from(">QQ", data, off + 8)
            else:
                offset, size = struct.unpack_from(">II", data, off + 8)
            slice_data = data[offset : offset + size]
            try:
                out |= binary_exports(slice_data)
            except ValueError:
                continue
        return out
    if magic in (MH_MAGIC, MH_MAGIC_64, MH_CIGAM, MH_CIGAM_64):
        return _macho_slice_exports(data, 0)
    raise ValueError(f"not a Mach-O file (magic {magic:#x})")


def binary_exports(data: bytes) -> set[str]:
    """Return the exported symbol names of a PE, ELF, Mach-O or ar binary blob."""
    if data[:2] == b"MZ":
        return pe_exports(data)
    if data[:4] == b"\x7fELF":
        return elf_exports(data)
    if data[:8] == b"!<arch>\n":
        out: set[str] = set()
        for member in _ar_members(data):
            try:
                out |= binary_exports(member)
            except ValueError:
                continue  # __.SYMDEF and other non-object members
        return out
    return macho_exports(data)


def normalize(names: set[str], strip_underscore: bool) -> set[str]:
    if not strip_underscore:
        return names
    return names | {n[1:] for n in names if n.startswith("_")}


def csharp_entry_points(sources: list[Path]) -> dict[str, dict]:
    """Map entry point name -> {locations, platforms}.

    ``platforms`` is None when the declaration is unconditional, otherwise the
    set of Unity platforms implied by the enclosing preprocessor condition
    (only the conditions this tool understands are tracked).
    """
    guard_platforms = {
        "UNITY_IOS": "ios",
        "UNITY_ANDROID": "android",
        "UNITY_STANDALONE_OSX": "macos",
        "UNITY_EDITOR_OSX": "macos",
        "UNITY_STANDALONE_WIN": "windows",
        "UNITY_EDITOR_WIN": "windows",
    }
    found: dict[str, dict] = {}
    for root in sources:
        if not root.exists():
            continue
        for cs in sorted(root.rglob("*.cs")):
            text = cs.read_text(encoding="utf-8", errors="replace")
            lines = text.splitlines()
            # precompute the platform guard active on each line
            active: list[set[str]] = []
            stack: list[set[str] | None] = []
            for line in lines:
                match = PREPROCESSOR_RE.match(line)
                if match:
                    directive, rest = match.group("directive"), match.group("rest")
                    if directive in ("if", "ifdef", "ifndef", "elif"):
                        token = rest.strip().split("(")[0].strip()
                        platforms = {
                            platform
                            for symbol, platform in guard_platforms.items()
                            if re.search(rf"\b{symbol}\b", rest)
                        }
                        if directive in ("ifdef", "ifndef") and token in guard_platforms:
                            platforms = {guard_platforms[token]}
                        if directive == "elif":
                            parent = stack[-1] if stack else None
                            platforms = (platforms | parent) if parent else platforms
                        stack.append(platforms or None)
                    elif directive == "else":
                        parent = stack.pop() if stack else None
                        stack.append(None)
                    elif directive == "endif":
                        if stack:
                            stack.pop()
                merged: set[str] = set()
                for entry in stack:
                    if entry:
                        merged |= entry
                active.append(merged)
            for match in DLLIMPORT_RE.finditer(text):
                entry_point = ENTRYPOINT_RE.search(match.group("args"))
                name = entry_point.group("name") if entry_point else match.group("name")
                line_index = text[: match.start()].count("\n")
                location = f"{cs.relative_to(REPO_ROOT).as_posix()}:{line_index + 1}"
                record = found.setdefault(name, {"locations": [], "platforms": None})
                record["locations"].append(location)
                platforms = active[line_index] if line_index < len(active) else set()
                if platforms:
                    record["platforms"] = platforms
    return found


def verify(platforms: list[str], json_path: Path | None, strict: bool) -> int:
    results: dict[str, object] = {"platforms": platforms, "plugins": {}}
    errors: list[str] = []
    known: list[str] = []

    for plugin, binaries in PLUGIN_BINARIES.items():
        declared = csharp_entry_points([REPO_ROOT / p for p in CSHARP_SOURCES[plugin]])
        plugin_report: dict[str, object] = {
            "declared_entry_points": len(declared),
            "platforms": {},
        }
        for platform in platforms:
            rel = binaries.get(platform)
            entry: dict[str, object] = {"binary": rel}
            if rel is None:
                entry["status"] = "no binary configured for platform"
                plugin_report["platforms"][platform] = entry
                continue
            path = REPO_ROOT / rel
            if not path.exists():
                entry["status"] = "binary missing"
                errors.append(f"{plugin} [{platform}]: binary not found: {rel}")
                plugin_report["platforms"][platform] = entry
                continue
            try:
                exports = normalize(binary_exports(path.read_bytes()), platform in ("ios", "macos"))
            except ValueError as exc:
                entry["status"] = f"unreadable: {exc}"
                errors.append(f"{plugin} [{platform}]: cannot read {rel}: {exc}")
                plugin_report["platforms"][platform] = entry
                continue

            gaps = KNOWN_PLATFORM_GAPS.get((plugin, platform), {})
            provided = PLATFORM_PROVIDED.get((plugin, platform), {})
            missing: list[str] = []
            missing_known: dict[str, str] = {}
            for name, record in sorted(declared.items()):
                if name in exports:
                    continue
                if record["platforms"] is not None and platform not in record["platforms"]:
                    continue  # declaration is compiled out on this platform anyway
                if name in provided:
                    missing_known[name] = f"provided by {provided[name]}"
                    continue
                reason = next((text for prefix, text in gaps.items() if name.startswith(prefix)), None)
                if reason is not None:
                    missing_known[name] = reason
                else:
                    missing.append(name)

            entry["status"] = "ok" if not missing else "missing entry points"
            entry["export_count"] = len(exports)
            entry["missing_count"] = len(missing)
            entry["missing"] = {name: declared[name]["locations"] for name in missing}
            entry["known_gaps"] = missing_known
            plugin_report["platforms"][platform] = entry
            for name in missing:
                errors.append(
                    f"{plugin} [{platform}]: {name} declared at "
                    f"{', '.join(declared[name]['locations'])} is not exported by {rel}"
                )
            for name, reason in missing_known.items():
                known.append(f"{plugin} [{platform}]: {name} ({reason})")
        results["plugins"][plugin] = plugin_report

    results["errors"] = errors
    results["known_gaps"] = known
    if json_path is not None:
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(results, indent=2), encoding="utf-8")

    for plugin, report in results["plugins"].items():  # type: ignore[union-attr]
        print(f"{plugin}: {report['declared_entry_points']} declared P/Invoke entry points")
        for platform, entry in report["platforms"].items():  # type: ignore[union-attr]
            count = entry.get("export_count", "-")
            note = f", {len(entry['known_gaps'])} known gap(s)" if entry.get("known_gaps") else ""
            print(f"  {platform:8s} {entry['status']:24s} exports={count}{note}")
    if known:
        print("\nKNOWN PLATFORM GAPS (expected, not failures):")
        for line in known:
            print(f"  - {line}")
    if errors:
        print("\nFAILURES:")
        for line in errors:
            print(f"  - {line}")
        return 1
    if strict and known:
        return 1
    print("\nAll declared entry points are exported by every checked binary.")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--platform",
        action="append",
        choices=sorted({p for b in PLUGIN_BINARIES.values() for p in b}),
        help="platform to verify (repeatable); default: every platform with a configured binary",
    )
    parser.add_argument("--json", type=Path, help="also write the full report as JSON to this path")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="also fail on the documented platform gaps in KNOWN_PLATFORM_GAPS",
    )
    args = parser.parse_args(argv)

    platforms = args.platform or ["windows", "android", "ios", "macos"]
    return verify(list(dict.fromkeys(platforms)), args.json, args.strict)


if __name__ == "__main__":
    sys.exit(main())
