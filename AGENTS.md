# Local browser access

- Chrome is installed and may be launched directly whenever local browser access is useful.
- An unavailable in-app browser connection does not mean that a local browser is unavailable; use the project's Chrome/Playwright harness or launch visible Chrome instead.
- Prefer visible Chrome when the user wants to watch browser testing.

# Git synchronization

- Always pull before pushing because CI may have committed replacement binaries upstream.
- CI replacement-binary commits are always safe to merge.
- Avoid stashing when a pull can safely merge without it. If a stash or autostash is necessary, inventory the dirty files first and verify that the same contents are restored immediately afterward; never leave the user's changes stranded in a stash.
