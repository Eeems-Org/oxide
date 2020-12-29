{ sources ? import ./nix/sources.nix { }
, system ? builtins.currentSystem
, pkgs ? import sources.nixpkgs { inherit system; }
}:

let
  inclusive = pkgs.callPackage "${sources.nix-inclusive}/inclusive.nix" { };
in

pkgs.callPackage ./oxide.nix { inherit inclusive; }
