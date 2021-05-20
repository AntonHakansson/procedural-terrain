{
  description = "Advanced Computer Graphics project";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils, }:
    utils.lib.eachDefaultSystem (system:
      let pkgs = import nixpkgs { inherit system; };
      in rec {
        packages.project = pkgs.stdenv.mkDerivation {
          pname = "AdvCompProject";
          version = "0.0.1";
          nativeBuildInputs = with pkgs; [
            pkgconfig
            cmake
            cmakeCurses
            cgdb
            clang-tools
            cmake-format
            cppcheck
            include-what-you-use
            cmake-language-server
          ];
          buildInputs = with pkgs; [ xorg.libX11 xorg.libXext libGL ];
          src = ./.;
          configurePhase = ''
            cmake -B build
          '';
          buildPhase = "cmake --build build";
        };

        defaultPackage = packages.project;
      });
}
