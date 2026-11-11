# Frequently Asked Questions

## Q: What is the development state of this project?
A: The application is still under active development.

---

## Q: Is the application stable?
A: To be transparent — the application is still under active development and **may be unstable** at times. We recommend saving your work frequently.

Use **"Ctrl+S"** to save, and **"Ctrl+Alt+S"** to save a timestamped backup copy. This helps protect your work in case of crashes or unexpected behavior.

Hesiod also keeps a **crash-recovery snapshot**: every two minutes (when something changed) the open project is written to `hesiod/autosave/` in your local application data folder (`~/.local/share` on Linux, `%LOCALAPPDATA%` on Windows, `~/Library/Application Support` on macOS; next to the executable in portable mode). If Hesiod did not shut down cleanly, the next launch offers to restore that snapshot, or to keep it for a later launch. It is removed whenever you save, open another project or quit normally. The interval and the feature itself can be changed under *Application Settings > Global*.

---

## Q: Why isn’t the application production-ready?
A: This project is **Free and Open Source Software (FOSS)**, and it’s currently developed and maintained by just **one person**. As a result, things may be rough around the edges, features might be incomplete, and bugs may occur.

I'm actively looking for **feedback, contributors, and help with everything** — development, testing, documentation, or just sharing your experience using the app. Any support is appreciated!

---

## Q: What’s the license, and can I use the results commercially?
A: Hesiod is released under the **GNU General Public License v3.0 (GPL-3.0)**.
You may use, study, modify and redistribute it, including selling copies, provided
you comply with the licence's distribution and source-code requirements. See the
[project licence](https://github.com/ottolink-dev/Hesiod/blob/main/LICENSE) and the
[GNU FAQ on selling GPL software](https://www.gnu.org/licenses/gpl-faq.en.html#DoesTheGPLAllowMoney).

Terrains and other assets you generate from your own inputs can be used in
personal or commercial projects, including selling those assets. The GPL does
not automatically apply to a program's output. Imported assets or presets can
have their own licence terms; see the
[GNU explanation of program output](https://www.gnu.org/licenses/gpl-faq.en.html#WhatCaseIsOutputGPL).

---

## Q: Are there any tutorials for Hesiod?
A: Yes. Start with [Your first terrain](../getting_started/index.md), then use
the [Guides](../guides/index.md) and [User Manual](../user_manual/index.md) for
specific workflows. The [Hesiod YouTube channel](https://www.youtube.com/@Hesiod-s9n)
has video walkthroughs of the interface and terrain viewer.

The downloaded application's `data/examples` folder contains example `.hsd`
projects. Older examples and videos can show previous node names or interface
layouts; use the current node reference when following them.

---

## Q: How big can I make the terrain in the app?
A: In the desktop application, terrain size is limited to **4k × 4k cells** to maintain responsive performance. Beyond that, computation becomes slow and can degrade the user experience.

To work around this, you can edit the terrain at a lower resolution (e.g., **1k × 1k**) in the GUI, then use **Hesiod's batch mode** (headless, no GUI) to generate higher-resolution output. In batch mode, the actual limit depends on your hardware—primarily your **RAM and GPU memory**. As a rough estimate, resolutions up to **32k × 32k** may be possible on powerful systems.
