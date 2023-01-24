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
[![Oxide v2.0-beta](https://i.imgur.com/1Q9A4NF.png)](https://youtu.be/rIRKgqy21L0 "Oxide v2.0-beta")

## Building

Install the reMarkable toolchain and then run `make release`. It will produce a folder named `release` containing all the output.

### Nix

Works on x86_64-linux or macOS with
[linuxkit-nix](https://github.com/nix-community/linuxkit-nix).

```ShellSession
$ nix build
```
