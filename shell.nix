{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "screentinker-mcu-dev";

  nativeBuildInputs = with pkgs; [
    pkg-config
    gnumake
  ];

  buildInputs = with pkgs; [
    # Host simulator dependencies
    SDL2
    curl

    # PlatformIO & embedded firmware tooling
    platformio
    python3
    git
    udev
  ];

  shellHook = ''
    export PKG_CONFIG_PATH="${pkgs.SDL2.dev}/lib/pkgconfig:${pkgs.curl.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"
    echo "=================================================================="
    echo "  ScreenTinker MCU & Host Simulator NixOS Development Shell"
    echo "=================================================================="
    echo "  • Build Simulator:  pio run -e simulator"
    echo "  • Run Simulator:    ./screentinker_sim [options]"
    echo "  • Build Firmware:   pio run -e seeed_reterminal_sticky"
    echo "=================================================================="
  '';
}
