{
  description = "dwm - dynamic window manager development environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      dwmTest = pkgs.writeShellScriptBin "dwm-test" ''
        #!/usr/bin/env bash
        set -euo pipefail

        DISPLAY_NUM=":99"

        Xephyr -ac -br -noreset -screen 1920x1080 "$DISPLAY_NUM" >/dev/null 2>&1 &
        XEPHYR_PID=$!

        cleanup() {
          kill "$XEPHYR_PID" 2>/dev/null || true
        }
        trap cleanup INT TERM EXIT

        make clean && make
        sleep 1
        DISPLAY="$DISPLAY_NUM" ./dwm
      '';
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          # X11 库
          xorg.libX11
          xorg.libXft
          xorg.libXinerama
          xorg.libXrender
          xorg.libXcomposite
          xorg.libXext
          xorg.libXcursor

          # 字体相关
          fontconfig
          freetype

          # 编译工具
          gcc
          gnumake

          # 测试工具
          xorg.xorgserver  # 包含 Xephyr 虚拟显示器
          dwmTest
        ];

        shellHook = ''
          echo "dwm 开发环境已加载"
          echo "运行 'make' 编译 dwm"
          echo "运行 'make install' 安装 dwm"
          echo "运行 'dwm-test' 启动虚拟显示器测试"
        '';
      };
    };
}
