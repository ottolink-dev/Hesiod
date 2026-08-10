# This project needs you!

It won't go anywhere without contributors.

## Where to contribute
- [Hesiod](https://github.com/otto-link/Hesiod): desktop application, GUI
- [HighMap](https://github.com/otto-link/HighMap): core library, algorithms

## Development branches

The project uses two main branches:

* **`main`** — stable branch containing released or release-ready code.
* **`dev`** — active development and integration branch.

**New development should target `dev`, not `main`.**

`main` is kept stable and is updated from `dev` when a new release is ready.

### Branch structure

```text
feature/* ──→ dev ──→ main
    │
    │ PR
    ▼
 review
```

## Creating a branch

Create your development branch from the latest `dev`:

```bash
git checkout dev
git pull
git checkout -b feature/my-feature
```

Use a descriptive branch name, for example:

```text
feature/color-gradient
feature/snow-simulation
fix/opencl-allocation
fix/rangebar-widget
refactor/data-provider
docs/installation
```

## Pull requests

All changes should be submitted through a Pull Request.

### For contributors

If you are not a member of the project organization:

1. Fork the repository.
2. Create a branch from `dev`.
3. Make your changes.
4. Test your changes.
5. Push your branch to your fork.
6. Open a Pull Request targeting **`dev`**.

```text
your fork
    │
    └── feature/my-feature
              │
              ▼
             PR
              │
              ▼
             dev
```

Please **do not target `main`** unless specifically requested.

### For project members

Members with repository access can create branches directly in the main repository:

```text
dev
 │
 └── feature/my-feature
          │
          ▼
         PR
          │
          ▼
         dev
```

Direct pushes to `main` should not be used.

## Keeping your branch up to date

Before submitting or updating a PR, make sure your branch is based on recent `dev` changes.

For example:

```bash
git fetch origin
git rebase origin/dev
```

## Commit messages

Please use clear and concise commit messages.

The project generally follows this style:

```text
feat: add color gradient attribute
fix: correct OpenCL buffer allocation
refactor: simplify data provider
test: add range bar tests
docs: update installation instructions
```

**The exact format is less important than making the history understandable.**

## Releases

Releases are made from `main`.

The general workflow is:

```text
feature/*
    │
    ▼
   dev
    │
    │ stabilization
    ▼
  main
    │
    ▼
 release/tag
```

Release versions are tagged on `main`, for example:

```text
v0.9.0
v0.9.1
v1.0.0
```

## AI-assisted development

AI-assisted development is welcome. The project does not require code to be written entirely by a human. any AI coding agents to assist with implementation. What matters is **human responsibility for every contribution**.

Every Pull Request must have a human sponsor who:

* Has personally reviewed the submitted changes.
* Understands the changes sufficiently to explain and defend them during review.
* Has tested the changes as appropriate.
* Takes responsibility for the contribution.
* Remains the point of contact throughout the review process.

### AI agents and GitHub accounts

AI agents may be used to generate or modify code, but contributions must be submitted under the human contributor's GitHub account. **Please do not submit Pull Requests, commits, or repository changes using an autonomous agent's own GitHub account or identity.**

## Report an issue or getting help

Use the issue and discussion threads.

