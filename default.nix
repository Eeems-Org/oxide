{ sources ? import ./nix/sources.nix { }
, system ? builtins.currentSystem
, pkgs ? import sources.nixpkgs { inherit system; }
}:

pkgs.callPackage ./oxide.nix { }
