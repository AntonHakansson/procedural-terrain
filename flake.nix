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
          LD_LIBRARY_PATH = with pkgs;
            "${xorg.libX11}/lib:${xorg.libXext}/lib:${xorg.libXinerama}/lib:${xorg.libXi}/lib:${xorg.libXrandr}/lib:${libglvnd}/lib";

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
            glslang
          ];
          buildInputs = with pkgs; [
            libglvnd
            xorg.libX11
            xorg.libXext
            xorg.libXinerama
            xorg.libXi
            xorg.libXrandr
          ];
          src = ./.;
          configurePhase = ''
            cmake -B build
          '';
          buildPhase = "cmake --build build";
        };

        defaultPackage = packages.project;
      });
}
