# [libogc2](https://github.com/extremscorner/libogc2)

[![libogc2 build](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml/badge.svg)](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml) [![Extrems' Corner.org Discord](https://discordapp.com/api/guilds/243509579961466881/widget.png)](https://discord.extremscorner.org/)

## Projects using libogc2

- [CleanRip](https://github.com/emukidid/cleanrip) - GameCube/Wii disc image creation tool
- [GCMM](https://github.com/suloku/gcmm) - GameCube memory card manager
- [gekkoboot](https://github.com/redolution/gekkoboot) - GameCube bootloader
- [Seta GX](https://github.com/fadedled/seta-gx) - Sega Saturn emulator
- [Swiss](https://github.com/emukidid/swiss-gc) - Swiss army knife of GameCube homebrew
- [Wii64/Cube64](https://github.com/emukidid/Wii64) - Nintendo 64 emulator
- [WiiSX/CubeSX](https://github.com/emukidid/pcsxgc) - PlayStation 1 emulator
- [Xeno Crisis](https://shop.bitmapbureau.com/collections/gamecube/products/xeno-crisis-gamecube) - Retro-style arena shooter

### Subprojects

[cubeboot-tools](https://github.com/extremscorner/cubeboot-tools), [gamecube-examples](https://github.com/extremscorner/gamecube-examples), [gamecube-tools](https://github.com/extremscorner/gamecube-tools), [libdvm](https://github.com/extremscorner/libdvm), [libfat](https://github.com/extremscorner/libfat), [libntfs](https://github.com/extremscorner/libntfs), [wii-examples](https://github.com/extremscorner/wii-examples)

## Installing

Follow the instructions from [pacman-packages](https://github.com/extremscorner/pacman-packages#readme).

```
sudo (dkp-)pacman -S libogc2 libogc2-docs libogc2-examples
```

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
