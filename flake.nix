{
  description = "Hesiod - node-based procedural terrain generator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    # Hesiod vendors its dependencies as git submodules (recursively). Asking
    # Nix to fetch ourselves with submodules makes `nix run github:...` work
    # without the caller having to append `?submodules=1`.
    self.submodules = true;
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # Base version comes from the root CMakeLists.txt (project VERSION); the
      # git suffix mirrors what `git describe` would have produced in a normal
      # checkout, since the sandbox has no .git to ask.
      # Only the parts CMake consumes. Keeping the packaging files, docs and
      # tests out of the source means editing them doesn't trigger a rebuild.
      # `github:` flake references silently yield empty submodule directories
      # (GitHub tarballs carry no gitlinks), so refuse to build such a tree.
      src =
        assert nixpkgs.lib.assertMsg (builtins.pathExists ./external/HighMap/CMakeLists.txt) ''
          Hesiod was fetched without its submodules. Use a git+https flake
          reference, e.g. nix run 'git+https://github.com/ottolink-dev/Hesiod?ref=main'
        '';
        nixpkgs.lib.fileset.toSource {
          root = ./.;
          fileset = nixpkgs.lib.fileset.unions [
            ./CMakeLists.txt
            ./cmake
            ./external
            ./Hesiod
            ./bridges
            ./LICENSE
          ];
        };

      baseVersion = "0.6.0";
      gitSuffix = self.shortRev or self.dirtyShortRev or "unknown";
      version = "${baseVersion}+${gitSuffix}";
    in
    {
      packages = forAllSystems (pkgs: rec {
        hesiod = pkgs.callPackage ./nix/package.nix { inherit src version; };
        default = hesiod;
      });

      apps = forAllSystems (pkgs: rec {
        hesiod = {
          type = "app";
          program = "${self.packages.${pkgs.system}.hesiod}/bin/hesiod";
        };
        default = hesiod;
      });

      overlays.default = final: prev: {
        hesiod = final.callPackage ./nix/package.nix { inherit src version; };
      };

      formatter = forAllSystems (pkgs: pkgs.nixfmt-tree);
    };
}
