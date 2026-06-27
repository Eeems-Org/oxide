[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4a69f96e44504f7286d92abec506881a)](https://www.codacy.com/gh/Eeems-Org/oxide/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Eeems-Org/oxide&amp;utm_campaign=Badge_Grade)
[![CodeFactor](https://www.codefactor.io/repository/github/eeems-org/oxide/badge)](https://www.codefactor.io/repository/github/eeems-org/oxide)
[![Maintainability](https://api.codeclimate.com/v1/badges/db8574df9b0b8a1100bc/maintainability)](https://codeclimate.com/github/Eeems/oxide/maintainability)
[![rm1](https://img.shields.io/badge/rM1-supported-green)](https://remarkable.com/store/remarkable)
[![rm2](https://img.shields.io/badge/rM2-supported-green)](https://remarkable.com/store/remarkable-2)
[![opkg](https://img.shields.io/badge/OPKG-oxide-blue)](https://toltec-dev.org/)
[![Discord](https://img.shields.io/discord/385916768696139794.svg?label=reMarkable&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/ATqQGfu)

# Oxide

A launcher application for the [reMarkable tablet](https://remarkable.com/).

Head over to the [releases](https://github.com/Eeems/oxide/releases) page for more information on the latest release. You can also see some (likely outdated) [screenshots here](https://github.com/Eeems/oxide/wiki/Screenshots).

Here is an outdated video of it in action:
[![Oxide v2.6](https://i.imgur.com/IA7wAsE.png[/img])](https://youtu.be/FdgWUUUST9o "Oxide v2.6")

You can find other (likely outdated) [videos here](https://github.com/Eeems/oxide/wiki/Videos).

## Building

### Binaries

 1. Install the [reMarkable toolchain](https://remarkable.guide/devel/toolchains.html#official-toolchain) for your device/version
 2. Run `make release` or `make FEATURES=sentry release`
 3. The built files can be found in the `release/` folder
 
 Another option is to use `make build-rm1`, `make build-rm2`, `make build-rmpp`, `make build-rmppm`, or `make build-rmppure` to use podman to build for a specific device.

### Package files

 Install [vbuild](https://github.com/Eeems/vbuild)

 - `make package-armv7` build packages for rM1/rM2 and place them in `release/armv7/`
 - `make deploy-armv7` build and deploy packages for rM1/rM2 to `/home/root/packages/armv7` on the device
 - `make install-armv7` build, deploy, and install base packages for rM1/rM2 on the device
 - `make package-aarch64` build packages for rMPP/rMPPM/rMPPure and place them in `release/aarch64/`
 - `make deploy-aarch64` build and deploy packages for rMPP/rMPPM/rMPPure to `/home/root/packages/aarch64` on the device
 - `make install-aarch64` build, deploy, and install base packages for rMPP/rMPPM/rMPPure on the device
