#!/usr/bin/env python3
"""Shared composition status vocabulary for visual smoke classifiers."""

from __future__ import annotations


COMPOSITION_CONTRACTS = {
    "full_depth": {
        "composition_contract": "depth_composition",
        "failure_status": "expected_failed",
    },
    "ordered_overlay": {
        "composition_contract": "ordered_overlay",
        "depth_interleaving": "not_claimed",
        "failure_status": "failed",
    },
    "render_only": {
        "composition_contract": "render_only",
        "depth_interleaving": "not_claimed",
        "failure_status": "unknown",
    },
}


def build_composition_fields(mode: str, rendering_succeeded: bool, composition_failures: list[str]) -> dict:
    contract = COMPOSITION_CONTRACTS[mode]
    if mode == "render_only":
        compositing = "not_tested"
        ordered_overlay = "not_tested"
        depth_composition = "not_tested"
        depth_interleaving = contract["depth_interleaving"]
    elif composition_failures:
        compositing = contract["failure_status"]
        ordered_overlay = "failed" if mode == "ordered_overlay" else "not_tested"
        depth_composition = "expected_failed" if mode == "full_depth" else "not_claimed"
        depth_interleaving = "expected_failed" if mode == "full_depth" else contract["depth_interleaving"]
    else:
        compositing = "success" if rendering_succeeded else "unknown"
        ordered_overlay = "success" if mode == "ordered_overlay" and rendering_succeeded else "not_tested"
        depth_composition = "success" if mode == "full_depth" and rendering_succeeded else "not_claimed"
        depth_interleaving = "success" if mode == "full_depth" and rendering_succeeded else contract["depth_interleaving"]

    return {
        "composition_mode": mode,
        "composition_contract": contract["composition_contract"],
        "depth_interleaving": depth_interleaving,
        "compositing": compositing,
        "ordered_overlay": ordered_overlay,
        "depth_composition": depth_composition,
    }


def classification_succeeded(status: dict) -> bool:
    return status.get("rendering") == "success" and status.get("compositing") in {
        "success",
        "expected_failed",
        "not_tested",
    }
