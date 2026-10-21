"""Inventory Quill metadata and stroke headers without allocating paint vertices.

The qbin layout follows bundled QBinReader.ReadDrawingData/ReadStroke/ReadVertex.
Reports contain project filenames, not absolute private collection paths.
"""

from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path


def layers(layer: dict, path: str = "", parent_visible: bool = True):
    path = f"{path}/{layer['Name']}"
    visible = parent_visible and layer.get("Visible", True)
    yield layer, path, visible
    if layer["Type"] == "Group":
        for child in layer["Implementation"]["Children"]:
            yield from layers(child, path, visible)


def drawing_headers(stream, offset: int, size: int):
    if offset < 0 or offset + 4 > size:
        raise ValueError(f"Drawing offset outside qbin: {offset}")
    stream.seek(offset)
    count = struct.unpack("<i", stream.read(4))[0]
    if count < 0 or count > (size - offset - 4) // 40:
        raise ValueError(f"Invalid stroke count {count} at {offset}")
    brushes, opacity = Counter(), Counter()
    points = 0
    for _ in range(count):
        header = stream.read(40)
        if len(header) != 40:
            raise ValueError("Truncated stroke header")
        brush, disable_opacity, _, vertices = struct.unpack_from("<hBBi", header, 32)
        if disable_opacity not in (0, 1) or vertices < 0 or stream.tell() + vertices * 56 > size:
            raise ValueError("Invalid stroke header or vertex bounds")
        brushes[brush] += 1
        opacity["disabled" if disable_opacity else "enabled"] += 1
        points += vertices
        stream.seek(vertices * 56, 1)
    return {"strokes": count, "points": points, "brushes": dict(brushes), "rotationalOpacity": dict(opacity)}


def inventory(project: Path, references: list[Path]):
    document = json.loads((project / "Quill.json").read_text(encoding="utf-8-sig"))
    sequence = document["Sequence"]
    qbin = project / "Quill.qbin"
    size = qbin.stat().st_size
    result = {
        "project": project.name, "qbinBytes": size,
        "metadata": sequence.get("Metadata"), "frameRate": sequence.get("Framerate"),
        "candidateReferences": sorted({p.name for p in references if p.parent == project
            or p.stem == project.name or (project.name.split('_')[0].isdigit()
                and p.name.startswith(f"{project.name.split('_')[0]}_"))}),
        "layerTypes": Counter(), "visiblePaintRates": Counter(), "paint": [],
    }
    cache = {}
    with qbin.open("rb") as stream:
        for layer, path, visible in layers(sequence["RootLayer"]):
            result["layerTypes"][layer["Type"]] += 1
            if layer["Type"] != "Paint":
                continue
            impl = layer["Implementation"]
            rate = impl.get("Framerate", 24)
            if visible:
                result["visiblePaintRates"][rate] += 1
            drawings = impl.get("Drawings", [impl] if "DataFileOffset" in impl else [])
            headers = []
            for drawing in drawings:
                offset = int(str(drawing["DataFileOffset"]), 16)
                if offset not in cache:
                    cache[offset] = drawing_headers(stream, offset, size)
                headers.append(cache[offset])
            result["paint"].append({
                "path": path, "effectivelyVisible": visible, "frameRate": rate,
                "frames": len(impl.get("Frames", [0])), "drawings": len(drawings),
                "strokes": sum(h["strokes"] for h in headers),
                "rotationalOpacityEnabled": sum(h["rotationalOpacity"].get("enabled", 0) for h in headers),
            })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path)
    parser.add_argument("--reference-root", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    references = list(args.source_root.rglob("*.imm"))
    for root in args.reference_root:
        references.extend(root.rglob("*.imm"))
    reports = [inventory(p.parent, references) for p in sorted(args.source_root.rglob("Quill.json"))]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"schema": "quill-source-inventory-v1", "projects": reports}, indent=2), encoding="utf-8")
    for report in reports:
        enabled = sum(p["rotationalOpacityEnabled"] for p in report["paint"])
        print(f"[IMM_QUILL_INVENTORY] {report['project']}: visible rates={dict(report['visiblePaintRates'])}, rotational-opacity strokes={enabled}, candidate references={len(report['candidateReferences'])}")


if __name__ == "__main__":
    main()
