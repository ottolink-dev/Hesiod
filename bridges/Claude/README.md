# Claude bridge — `hesiod-generate` skill

This directory packages the **`hesiod-generate`** agent skill that drives Hesiod's
headless procedural-generation toolkit (`scripts/hsd/`) from an LLM coding agent.

It lives under `bridges/` so the Hesiod repository itself stays tool-agnostic: the
toolkit in `scripts/hsd/` is plain, dependency-free Python that anyone can call, and this
bridge is the Claude-specific packaging layered on top — a sibling to any future
`bridges/Blender`, `bridges/Unreal`, etc.

```
bridges/Claude/.claude/skills/hesiod-generate/
├── SKILL.md                      # the skill: workflow + rules the agent follows
└── reference/
    ├── spec-schema.md            # the compact .hsd spec format
    ├── recipes.md                # verified, ready-to-run example specs
    └── specs/*.json              # the example spec files themselves
```

## Installing the skill

Installing a skill means making `SKILL.md` discoverable by your agent. Copy (or symlink)
the `hesiod-generate/` directory into the agent's skill directory:

| Agent | Skill directory | Scope |
|---|---|---|
| Claude Code | `<project>/.claude/skills/` | this project only |
| Claude Code | `~/.claude/skills/` | all projects |
| Other agents that load Markdown skills | the agent's skill directory | per that agent's docs |

For example, to make it available in any project under Claude Code:

```sh
cp -r bridges/Claude/.claude/skills/hesiod-generate ~/.claude/skills/
# or, to track edits in one place, symlink it:
ln -s "$PWD/bridges/Claude/.claude/skills/hesiod-generate" ~/.claude/skills/hesiod-generate
```

For local development on this repo, a project-level symlink keeps a single source of
truth — edits to the bridge are what the agent loads, with nothing to copy by hand:

```sh
mkdir -p .claude/skills
ln -s ../../bridges/Claude/.claude/skills/hesiod-generate .claude/skills/hesiod-generate
```

(Add `/.claude/` to `.git/info/exclude` so the local symlink stays out of `git status`.)

## Runtime expectations

The skill shells out to the toolkit and assumes it runs **from the Hesiod repository
root**, with the package on the path:

```sh
PYTHONPATH=scripts python3 -m hsd <subcommand> [args]
```

So `scripts/hsd/` must be reachable. The reference spec paths in `SKILL.md` and
`recipes.md` are written relative to this repository root
(`bridges/Claude/.claude/skills/hesiod-generate/reference/specs/…`). If you install the
skill elsewhere, run the commands from a Hesiod checkout and adjust those spec paths
accordingly.

See also the user-facing guides:
[LLM-driven procedural generation](../../docs/guides/llm-procedural-generation.md) and
[Batch & headless](../../docs/guides/batch-headless.md).
