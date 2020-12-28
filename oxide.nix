{ stdenv, fetchFromGitHub, qt512, remarkable-toolchain }:

stdenv.mkDerivation {
  name = "oxide";
  src = stdenv.lib.cleanSource ./.;
  nativeBuildInputs = [ qt512.full remarkable-toolchain ];
  
  buildPhase = ''
    source ${remarkable-toolchain}/environment-setup-cortexa9hf-neon-oe-linux-gnueabi
    make release -j$NIX_BUILD_CORES
  '';
  
  installPhase = ''
    cp -r release/. $out
  '';
  
  meta = with stdenv.lib; {
    description = " A launcher application for the reMarkable tablet ";
    platform = [ "x86_64-linux" ];
    maintainers = [ maintainers.siraben ];
    license = licenses.mit;
  };
}
