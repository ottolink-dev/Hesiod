# Bridges

Integrations that connect Hesiod to external tools and agents, kept separate from the
core engine so the repository stays tool-agnostic.

Each bridge is a self-contained subdirectory with its own `README.md` explaining what it
is and how to install or use it.

| Bridge | What it provides |
|---|---|
| [`Blender/`](Blender/README.md) | One-way synchronization from Hesiod to Blender. |
| [`Claude/`](Claude/README.md) | The `hesiod-generate` agent skill — drives the headless `scripts/hsd` procedural-generation toolkit from an LLM coding agent. |

Future bridges (e.g. Unreal, Unity...) would live here as siblings.
