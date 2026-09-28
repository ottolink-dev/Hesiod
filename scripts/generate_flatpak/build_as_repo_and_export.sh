flatpak-builder -y --user --force-clean --repo=repo/ build/ io.github.hesiod.yml
flatpak build-bundle repo/ io.github.hesiod.flatpak io.github.hesiod
