{
  description = "ScreenTinker MCU firmware and generic host simulator environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = import ./shell.nix { inherit pkgs; };

        packages.screentinker_sim = pkgs.stdenv.mkDerivation {
          pname = "screentinker_sim";
          version = "1.2.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            pkg-config
            gnumake
          ];

          buildInputs = with pkgs; [
            SDL2
            curl
          ];

          buildPhase = ''
            make -C simulator
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp screentinker_sim $out/bin/
          '';
        };

        packages.default = self.packages.${system}.screentinker_sim;
      }
    );
}
