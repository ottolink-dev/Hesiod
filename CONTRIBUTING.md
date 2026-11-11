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


## Documentation and weekly CI

`.github/workflows/documentation.yml` builds documentation for relevant PRs and
pushes to `dev` and `main`. On `main`, release publication/edits, and manual runs
from `main`, it also builds the website's themed documentation from current
`main`, using `ottolink-dev/website/docs-build/build_docs.py`. It commits only
`public/docs` to the website's `main`; that repository's existing Cloudflare
workflow deploys the update. Release notes are already read from GitHub by the
website Worker. Development docs remain downloadable as Actions artifacts.
This builds committed documentation; generating screenshots with the desktop
application remains the separate `scripts/update_doc.sh` authoring step.

`.github/workflows/weekly-builds.yml` resolves `dev` once and builds that exact
commit and its pinned submodules for Linux x86-64 and Windows x86-64. It runs
Mondays at 04:23 UTC. A manual run defaults to building artifacts only; select
`publish` to update the website. Scheduled workflows must be present on `main`
(GitHub's default-branch requirement), even though they build `dev`.

Linux ships an AppImage plus runtime data in a ZIP. Windows uses MSVC 2022,
Qt's MSVC distribution, and a pinned vcpkg baseline; its ZIP contains the EXE,
runtime data, dependency DLLs, Qt plugins and WebEngine helper/resources.
Both packages include `SOURCE.txt` and the project license. Linux runs `--help`
before upload. Windows checks executable imports and loads the packaged DLLs
with development paths removed; running `--help` requires an OpenCL device
because Hesiod initializes OpenCL before parsing arguments. These checks do
not test graphics, GPU support, or interactive functionality.
Linux packages are built on Ubuntu 24.04 and need a compatible glibc; AppImage
does not remove that requirement. Windows users need their GPU's OpenCL driver.

### One-time maintainer setup

Create an Actions environment named `website` in `ottolink-dev/Hesiod`:

| Kind | Name | Value |
| --- | --- | --- |
| Variable | `WEBSITE_APP_ID` | GitHub App ID, installed on `ottolink-dev/website` with Contents read/write |
| Secret | `WEBSITE_APP_PRIVATE_KEY` | That App's PEM private key |
| Variable | `WEBSITE_R2_ENDPOINT` | `https://<account-id>.r2.cloudflarestorage.com` |
| Variable | `WEBSITE_R2_BUCKET` | Bucket bound as `RELEASES` in the website Worker |
| Secret | `WEBSITE_R2_ACCESS_KEY_ID` | R2 S3 credential scoped to that bucket |
| Secret | `WEBSITE_R2_SECRET_ACCESS_KEY` | Matching R2 secret with object read/write/delete access |

The App identity must be allowed to push generated documentation to website
`main`. An App token is necessary because Hesiod's `GITHUB_TOKEN` cannot write
to another repository, and pushes using it do not trigger ordinary workflows.
Protect the environment so only trusted workflow refs can use its credentials.
Forks and PRs never publish. No credentials are needed for build-only runs.

Weekly publication uses the website's existing version-2 `manifest.json` schema
and `dev` channel. Both builds must succeed before publication starts. The
publisher validates both archives, uploads to unique keys, checks the uploads,
then switches both download pointers in one conditional manifest write. Stable
builds and other platforms are preserved. Superseded files are removed only
after publication succeeds. Failed or conflicting publication may leave
unreferenced uploads; it does not delete potentially published files on an
ambiguous network failure. Inspect the manifest before removing those objects.
Avoid manual admin uploads during the CI publication window: CI detects changes
before its conditional write, but the existing admin uploader does not itself
use conditional writes.

A failed documentation build or rejected Git push leaves the deployed website
unchanged; inspect the Actions log and rerun after resolving the failure. If a
website deploy fails after the docs commit, rerun that repository's deployment.
An R2 cleanup warning means the new builds are live but an old object remains.
Build artifacts are retained for 14 days even when publishing fails.

Before enabling scheduled publishing, run the workflow manually without
publishing and test both downloaded packages on clean target machines. Then
configure the environment and run it with publication enabled. These workflows
must pass on hosted runners; local syntax checks cannot validate Windows or
Cloudflare credentials.

Local publisher regression checks:

```bash
python3 -m unittest discover -s tests/ci -v
```

### Publishing an existing macOS development build

Run **Publish macOS development build** from `main`, supplying the successful
macOS build's run ID and full head commit SHA. This publishes the selected
`hesiod-macos-arm64` artifact without rebuilding it. The workflow verifies the
run, archive checksum, source provenance and dependency-audit report before
updating only the website's macOS Dev download. Stable and Windows/Linux
packages are preserved. It uses the existing `website` environment's R2
credentials. PR test runs are accepted only from this repository, and their
packaged merge commit must match the selected head. The same conditional
manifest update protects against concurrent publication.
