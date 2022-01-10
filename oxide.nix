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
  ];

  preBuild = ''
    source ${remarkable-toolchain}/environment-setup-cortexa9hf-neon-remarkable-linux-gnueabi
  '';

  enableParallelBuilding = true;
  makeFlags = [ "release" ];

  installPhase = ''
    install -D -m 755 -t $out/opt/bin release/opt/bin/erode
    install -D -m 644 -t $out/opt/etc/draft/icons release/opt/etc/draft/icons/erode.svg
    install -D -m 644 -t $out/opt/etc/draft/icons release/opt/etc/draft/icons/erode-splash.png
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.erode.oxide
    install -D -m 755 -t $out/opt/bin release/opt/bin/fret
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.fret.oxide
    install -D -m 755 -t $out/opt/bin release/opt/bin/oxide
    install -D -m 644 -t $out/opt/etc release/opt/etc/oxide.conf
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.oxide.oxide
    install -D -m 644 -t $out/opt/etc/draft/icons release/opt/etc/draft/icons/oxide-splash.png
    install -D -m 755 -t $out/opt/bin release/opt/bin/rot
    install -D -m 644 -t $out/etc/dbus-1/system.d release/etc/dbus-1/system.d/codes.eeems.oxide.conf
    install -D -m 644 -t $out/lib/systemd/system release/etc/systemd/system/tarnish.service
    install -D -m 755 -t $out/opt/bin release/opt/bin/tarnish
    install -D -m 755 -t $out/opt/bin release/opt/bin/decay
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.decay.oxide
    install -D -m 755 -t $out/opt/bin release/opt/bin/corrupt
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.corrupt.oxide
    install -D -m 755 -t $out/opt/bin release/opt/bin/anxiety
    install -D -m 644 -t $out/opt/usr/share/applications release/opt/usr/share/applications/codes.eeems.anxiety.oxide
    install -D -m 644 -t $out/opt/etc/draft/icons release/opt/etc/draft/icons/image.svg
    install -D -m 644 -t $out/opt/etc/draft/icons release/opt/etc/draft/icons/anxiety-splash.png
  '';

  meta = with lib; {
    description = "A launcher application for the reMarkable tablet";
    platform = [ "x86_64-linux" ];
    maintainers = [ maintainers.siraben ];
    license = licenses.mit;
  };
}
