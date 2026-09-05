# Copilot Instructions for OTthing Firmware

This repository contains ESP/PlatformIO firmware, a web UI in `data/index.html`, and a FastAPI mock server in `tools/mock_otthing.py`.

## Working model

- Firmware (`src/`, `include/`) is the source of truth for naming and semantics.
- The web UI (`data/index.html`) consumes status/config and renders hero cards, status labels, curves, and controls.
- The mock server (`tools/mock_otthing.py`) emulates firmware endpoints and should closely match the runtime data structure.

## Project priorities

- Keep field naming consistent and canonical.
- Do not add spelling migration/fallback logic unless explicitly requested.
- Keep changes minimal, local, and focused on the request.
- Avoid broad refactors unless required to fix an actual issue.

## Canonical naming and schema rules

- Use dotted status paths exactly as produced by the firmware (`heatercircuit.0.roomsetpoint`, etc.).
- Treat status/config names as case-sensitive and canonical.
- If mock fields differ from firmware names, update the mock to use the firmware names (not vice versa) unless explicitly requested.
- Do not add alias keys (for example, camelCase and lowercase duplicates) unless explicitly requested.

## High-value conventions

- UI status field bindings use dotted paths like `heatercircuit.$.roomsetpoint`.
- Mock status paths must mirror firmware/runtime paths exactly.
- If adding mock controls, ensure:
  - `FIELDS` includes editable keys.
  - The `state["status"]` seed contains representative values.
  - The top quick editor, card controls, and JSON viewer stay in sync after updates.
- If changing value coloring in `data/index.html`, preserve the explicit rule: no valid range => no color.

## File-specific guidance

- `data/index.html`
  - Keep hero cards, pinned cards, and status labels aligned to the same field semantics.
  - Reuse shared resolver functions rather than duplicating path traversal logic.
  - For graph behavior, verify required status values and config flags are present.
  - Keep path usage consistent (`heatercircuit`, `roomcompInteg`, `retLimitInteg`, etc.).
- `tools/mock_otthing.py`
  - `FIELDS` drives admin controls and card order.
  - Keep section ordering intentional (Other, Master, Slave, then heater circuits if configured that way).
  - The JSON viewer interactions should support selecting a path for quick editing.
  - The `sendValue` success path should refresh controls and JSON boxes coherently.
- `src/` and `include/`
  - Use these files to confirm field names and bit/flag semantics before changing UI or mock fields.

## Common pitfalls

- Adding incorrect nested objects to mock status (for example, `roomComp.{...}`) when firmware uses flat fields (`roomcompInteg`).
- Updating only mock controls without adding default seed values in `state["status"]`.
- Updating quick editor behavior without syncing card controls or the JSON viewer.


## Build/run quick commands

- Build firmware release: `platformio run --environment release`
- Run mock server: `python tools/mock_otthing.py`

## Validation checklist for edits

1. No syntax errors in edited files.
2. New or updated mock fields are editable via the card controls and the top quick path editor.
3. The JSON viewer updates after value changes.
4. If graph behavior is affected, confirm that the required config and status keys exist.
5. If naming was touched, verify canonical spelling is used consistently.

## Style

- Keep code and text ASCII unless the file already uses Unicode.
- Keep comments short and use them only where the code is not obvious.
- Preserve existing formatting and structure.
- Prefer the smallest safe patch over broad reformatting.
