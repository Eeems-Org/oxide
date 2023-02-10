{ stdenv, lib, fetchFromGitHub, inclusive, qt512, remarkable-toolchain }:

stdenv.mkDerivation {
  name = "oxide";
  
  nativeBuildInputs = [ qt512.full remarkable-toolchain ];

  src = inclusive ./. [
    ./Makefile
    ./applications
    ./assets
    ./interfaces
    ./shared
    ./oxide.pro
    ./qmake
  ];

  preBuild = ''
    source ${remarkable-toolchain}/environment-setup-cortexa9hf-neon-remarkable-linux-gnueabi
  '';

  enableParallelBuilding = true;
  makeFlags = [ "release" ];

  installPhase = ''
    cp -a release/. $out
  '';

  meta = with lib; {
    description = "A launcher application for the reMarkable tablet";
    platform = [ "x86_64-linux" ];
    maintainers = [ maintainers.siraben ];
    license = licenses.mit;
  };
}
