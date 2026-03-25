# AGENTS.md

## Purpose

Repository guidance for coding agents working in this repo.

## Default Workflow

1. Make the requested changes.
2. Run the most relevant verification available for the changed area.
3. If verification passes, create a git commit.
4. Push the commit to `origin` on the current branch.

## Push Policy

- After a successful change, agents should push to `origin` by default.
- Use a normal non-interactive flow:
  - `git add ...`
  - `git commit -m "<clear message>"`
  - `git push origin HEAD`
- If the push is rejected because the remote moved, stop and report it instead of force-pushing.
- Never use `git push --force` or `git push --force-with-lease` unless the user explicitly asks for it.

## Safety Rules

- Do not push if verification clearly failed.
- Do not revert unrelated user changes.
- Do not amend existing commits unless the user explicitly asks.
- If credentials, branch protections, or remote permissions block the push, report the blocker clearly.

## Communication

- In the final response, state whether the agent:
  - changed files,
  - ran verification,
  - created a commit,
  - pushed to `origin`.
