import glob
import os
import requests
import shutil

# ----------------------------
# Config
# ----------------------------
GITHUB_BASE_URL = "https://raw.githubusercontent.com/marella/material-design-icons/main/svg/outlined"

OUT_DIR = "Hesiod/data/icons/"

ICONS = [
    "account_tree", "add", "bakery_dining", "bookmark", "check",
    "cloud_download", "exit_to_app", "file_open", "hdr_strong", "help", "home",
    "info", "landscape", "link", "palette", "public", "push_pin", "refresh",
    "restore", "save", "save_as", "scatter_plot", "settings",
    "settings_backup_restore", "tune", "u_turn_left", "visibility",
    "visibility_off", "waves"
]
ICONS_ACCENT = ["push_pin", "visibility"]
ICONS_DISABLED = ["push_pin", "visibility_off"]

COLOR = '#F4F4F5'
COLOR_ACCENT = '#5E81AC'
COLOR_DISABLED = '#949495'

# ----------------------------
# Functions
# ----------------------------


def recolor_svg_file(file_path, color):
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Replace <path with <path fill="COLOR"
        colored_content = content.replace('<path ', f'<path fill="{color}" ')

        with open(file_path, "w", encoding="utf-8") as f:
            f.write(colored_content)

        print(f"File recolored successfully: {file_path}")

    except Exception as e:
        print(f"Error recoloring {file_path}: {e}")


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)

    # --- starts with non-material icons

    path = '/home/glop/Documents/dev/Hesiod/scripts/generate_icons/icon_others/'
    fnames = glob.glob(os.path.join(path, '*.svg'))

    for fname in fnames:
        fname_dst = os.path.join(OUT_DIR, os.path.basename(fname))
        shutil.copy(fname, fname_dst)

        recolor_svg_file(fname_dst, COLOR)

    # --- download non-material icons

    for icon in ICONS:
        svg_url = f"{GITHUB_BASE_URL}/{icon}.svg"  # default size 24px
        response = requests.get(svg_url)
        if response.status_code != 200:
            print(f"Failed to download {icon}: {response.status_code}")
            continue
        svg_content = response.text

        fname = os.path.join(OUT_DIR, f"{icon}.svg")

        with open(fname, "w", encoding="utf-8") as f:
            f.write(svg_content)

        if icon in ICONS_ACCENT:
            fname_accent = os.path.join(OUT_DIR, f"{icon}_accent.svg")
            shutil.copy(fname, fname_accent)
            recolor_svg_file(fname_accent, COLOR_ACCENT)

        if icon in ICONS_DISABLED:
            fname_accent = os.path.join(OUT_DIR, f"{icon}_disabled.svg")
            shutil.copy(fname, fname_accent)
            recolor_svg_file(fname_accent, COLOR_DISABLED)

        recolor_svg_file(fname, COLOR)
