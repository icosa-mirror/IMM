# Local browser access

- Chrome is installed and may be launched directly whenever local browser access is useful.
- An unavailable in-app browser connection does not mean that a local browser is unavailable; use the project's Chrome/Playwright harness or launch visible Chrome instead.
- Never describe browser or visual verification as unavailable solely because the in-app automation backend is unavailable.
- Prefer visible Chrome when the user wants to watch browser testing.

# Git synchronization

- Always pull before pushing because CI may have committed replacement binaries upstream.
- CI replacement-binary commits are always safe to merge.
- Avoid stashing when a pull can safely merge without it. If a stash or autostash is necessary, inventory the dirty files first and verify that the same contents are restored immediately afterward; never leave the user's changes stranded in a stash.

# Critical blockers and goal continuation

- If continued progress depends on an important user approval, decision, or other blocker, never mention it only once in a live update and then continue as though it was communicated.
- Either stop work and make the blocker plus the exact response needed the terminal response, or continue useful work while repeating the blocker and exact response needed on every line of every user-visible update until the user answers, so it cannot scroll past unnoticed.
- Treat the blocked part of the goal as paused until the user explicitly answers. Do not claim that the user failed to answer a request that appeared only in an earlier, potentially scrolled-off update.
