#!/usr/bin/env python3
"""Delete selected GitHub Actions artifacts from the current workflow run."""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request


def request_json(url: str, token: str) -> dict:
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def delete_url(url: str, token: str) -> int:
    request = urllib.request.Request(
        url,
        method="DELETE",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return response.status
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            return exc.code
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--run-id", default=os.environ.get("GITHUB_RUN_ID", ""))
    parser.add_argument("--token", default=os.environ.get("GITHUB_TOKEN", ""))
    parser.add_argument("--name", action="append", default=[])
    parser.add_argument("--prefix", action="append", default=[])
    args = parser.parse_args()

    if not args.repo or not args.run_id or not args.token:
        print("Missing repo, run-id, or token; skipping artifact cleanup")
        return 0

    wanted_names = set(args.name)
    wanted_prefixes = tuple(args.prefix)
    if not wanted_names and not wanted_prefixes:
        print("No artifact names or prefixes requested; skipping artifact cleanup")
        return 0

    api_root = f"https://api.github.com/repos/{args.repo}/actions/runs/{args.run_id}/artifacts?per_page=100"
    deleted = 0
    page = 1
    while True:
        data = request_json(f"{api_root}&page={page}", args.token)
        artifacts = data.get("artifacts", [])
        if not artifacts:
            break
        for artifact in artifacts:
            name = str(artifact.get("name", ""))
            if name in wanted_names or any(name.startswith(prefix) for prefix in wanted_prefixes):
                artifact_id = artifact.get("id")
                if artifact_id is None:
                    continue
                status = delete_url(f"https://api.github.com/repos/{args.repo}/actions/artifacts/{artifact_id}", args.token)
                print(f"Deleted artifact {name} ({artifact_id}) status={status}")
                deleted += 1
        if len(artifacts) < 100:
            break
        page += 1

    print(f"Deleted {deleted} artifact(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
