{
  description = "A very basic flake";
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { self
    , nixpkgs
    , flake-utils
    ,
    }:
    let
      hostname = config.networking.hostName;
      overlay =
        final: prev: {
          dwm = prev.dwm.overrideAttrs (oldAttrs: rec {
            postPatch = (oldAttrs.postPatch or "") + ''
              cp ./config/${hostname}.h ./config.h
            '';
            version = "master";
            src = ./.;
          });
        };
    in
    flake-utils.lib.eachDefaultSystem
      (
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
            overlays = [
              self.overlays.default
            ];
          };
        in
        rec {
          packages.dwm = pkgs.dwm;
          packages.default = pkgs.dwm;
          devShells.default = pkgs.mkShell {
            buildInputs = with pkgs; [ xorg.libX11 xorg.libXft xorg.libXinerama gcc ];
          };
        }
      )
    // { overlays.default = overlay; };
}
