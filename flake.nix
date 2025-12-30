{
  description = "dwm - dynamic window manager development environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
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

          # 字体相关
          fontconfig
          freetype

          # 编译工具
          gcc
          gnumake

          # 测试工具
          xorg.xorgserver  # 包含 Xephyr 虚拟显示器
          xorg.xdpyinfo
        ];

        shellHook = ''
          echo "dwm 开发环境已加载"
          echo "运行 'make' 编译 dwm"
          echo "运行 'make install' 安装 dwm"
          echo "运行 'make test' 或 'nix develop -c make clean test' 启动虚拟显示器测试"
        '';
      };
    };
}
