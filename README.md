# [libogc2](https://github.com/extremscorner/libogc2)

[![libogc2 build](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml/badge.svg)](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml) [![Extrems' Corner.org Discord](https://discordapp.com/api/guilds/243509579961466881/widget.png)](https://discord.extremscorner.org/)

**libogc2** contains wide-ranging fixes and additions to most subsystems of libogc, provides hardware enablement for most new (and old) Nintendo GameCube accessories on the market or in prototype stage, and enables greater interoperability with [Swiss](https://github.com/emukidid/swiss-gc).

It is largely API compatible with libogc 2.1.0 and earlier.

## Projects using libogc2

- [240p Test Suite](https://github.com/ArtemioUrbina/240pTestSuite) - Video suite for game consoles
- [CleanRip](https://github.com/emukidid/cleanrip) - Nintendo GameCube/Wii disc image creation tool
- [FCE Ultra GX](https://github.com/dborth/fceugx) - Nintendo Entertainment System emulator
- [FlippyDrive](https://www.crowdsupply.com/team-offbroadway/flippydrive) - Nintendo GameCube optical drive emulator
- [GCC Test Suite](https://github.com/greenwave-1/GTS) - Nintendo GameCube controller tester
- [GCMM](https://github.com/suloku/gcmm) - Nintendo GameCube memory card manager
- [gekkoboot](https://github.com/redolution/gekkoboot) - Nintendo GameCube bootloader
- [Seta GX](https://github.com/fadedled/seta-gx) - Sega Saturn emulator
- [Snes9x GX](https://github.com/dborth/snes9xgx) - Super Nintendo Entertainment System emulator
- [Swiss](https://github.com/emukidid/swiss-gc) - Swiss army knife of GameCube homebrew
- [Visual Boy Advance GX](https://github.com/dborth/vbagx) - Game Boy Advance emulator
- [WakeMii](https://github.com/emukidid/wakemii) - Alarm clock music player
- [Wii64/Cube64](https://github.com/emukidid/Wii64) - Nintendo 64 emulator
- [WiiSX/CubeSX](https://github.com/emukidid/pcsxgc) - PlayStation 1 emulator
- [Xeno Crisis](https://shop.bitmapbureau.com/collections/gamecube/products/xeno-crisis-gamecube) - Retro-style arena shooter

### Unofficial ports

- [Nicky Boom](https://github.com/emukidid/Nicky-Boum) - Side-scrolling platformer
- [Raptor: Call of the Shadows](https://github.com/RetroGamer02/raptor-consoles/tree/ppc-sys) - Vertically scrolling shooter

### Subprojects

[cubeboot-tools](https://github.com/extremscorner/cubeboot-tools), [gamecube-examples](https://github.com/extremscorner/gamecube-examples), [gamecube-tools](https://github.com/extremscorner/gamecube-tools), [libansnd](https://github.com/extremscorner/libansnd), [libdvm](https://github.com/extremscorner/libdvm), [libfat](https://github.com/extremscorner/libfat), [libntfs](https://github.com/extremscorner/libntfs), [opengx](https://github.com/extremscorner/opengx), [SDL](https://github.com/extremscorner/SDL), [wii-examples](https://github.com/extremscorner/wii-examples)

## Installing

Follow the instructions from [pacman-packages](https://github.com/extremscorner/pacman-packages#readme).

```
sudo (dkp-)pacman -S libogc2 libogc2-docs libogc2-examples
```

When asked for a `libogc2-libfat` provider, `libogc2-libdvm` is generally preferred, but `libogc2-libfat` may be chosen if you have concerns regarding exFAT's legal status.
If you have a need for `libogc2-libdvm` without exFAT support, please open an issue.

## Migrating from libogc

### GNU Make

```diff
-include $(DEVKITPPC)/gamecube_rules
+include $(DEVKITPRO)/libogc2/gamecube_rules
```

```diff
-include $(DEVKITPPC)/wii_rules
+include $(DEVKITPRO)/libogc2/wii_rules
```

### CMake

```
sudo (dkp-)pacman -S libogc2-cmake
```

`DKP_OGC_PLATFORM_LIBRARY` may be set to `libogc` or `libogc2` to build projects using either thereafter.

## Building

1. Existing packages should first be uninstalled if already installed.

   ```
   sudo (dkp-)pacman -R --cascade libogc2(-git)
   ```

2. Install build dependencies.

   ```
   sudo (dkp-)pacman -S --needed devkitPPC gamecube-tools ppc-libmad
   ```

3. Clone and build source repositories.

   ```
   git clone https://github.com/extremscorner/libogc2.git
   cd libogc2
   make
   sudo -E make install
   ```

   ```
   git clone https://github.com/extremscorner/libfat.git
   cd libfat
   make ogc-release
   sudo -E make ogc-install
   ```

Using `sudo` is not necessary with MSYS2.

## Upgrading

```
sudo (dkp-)pacman -Syu
```

```
cd libogc2
git pull
sudo -E make uninstall
sudo -E make clean
make
sudo -E make install
```

```
cd libfat
git pull
make ogc-clean
make ogc-release
sudo -E make ogc-install
```
