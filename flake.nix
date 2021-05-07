{
  description = "Advanced Computer Graphics project";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils, }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in rec {
        devShell = pkgs.mkShell {
          name = "advg";
          nativeBuildInputs = with pkgs; [ cmake gdb cgdb vscode ];
          buildInputs = with pkgs; [ SDL2 glm glew libGL ];
        };
        packages = {
          project = pkgs.stdenv.mkDerivation {
            name = "advg";
            nativeBuildInputs = with pkgs; [ cmake ];
            src = ./.;
          };
        };
        defaultPackage = packages.javalette;
      });
}
