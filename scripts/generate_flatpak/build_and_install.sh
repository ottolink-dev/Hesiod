flatpak uninstall --noninteractive --force-remove -y io.github.hesiod
flatpak-builder -y --user --force-clean --install build/ io.github.hesiod.yml
flatpak run io.github.hesiod
