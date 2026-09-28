# flatpak run --command=git org.flatpak.Builder config --global url."https://github.com/".insteadOf "git@github.com:"
flatpak run org.flatpak.Builder --force-clean --user --repo repo/ build/ io.github.hesiod.yml
