{
  description = "MSI MEG CoreLiquid S360 Linux driver (hidapi + lm_sensors)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    systems = ["x86_64-linux"];
    forAllSystems = f:
      nixpkgs.lib.genAttrs systems (system: let
        pkgs = import nixpkgs {inherit system;};
      in
        f pkgs system);
  in {
    devShells = forAllSystems (pkgs: system: {
      default = pkgs.mkShell {
        packages = [
          pkgs.gcc
          pkgs.hidapi
          pkgs.lm_sensors
          pkgs.pkg-config
        ];
      };
    });
  };
}
