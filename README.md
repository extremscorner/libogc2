# libogc2

[![libogc2 build](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml/badge.svg)](https://github.com/extremscorner/libogc2/actions/workflows/continuous-integration-workflow.yml) [![Extrems' Corner.org Discord](https://discordapp.com/api/guilds/243509579961466881/widget.png)](https://discord.extremscorner.org/)

## Projects using libogc2

- [GCMM](https://github.com/suloku/gcmm) - GameCube memory card manager
- [iplboot](https://github.com/redolution/iplboot) - Minimal GameCube IPL
- [Swiss](https://github.com/emukidid/swiss-gc) - The swiss army knife of GameCube homebrew
- [Wii64/Cube64](https://github.com/emukidid/Wii64) - Nintendo 64 emulator
- [WiiSX/CubeSX](https://github.com/emukidid/pcsxgc) - PlayStation 1 emulator

## Installing

```
sudo (dkp-)pacman -S ppc-libmad
```

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

Not currently supported.

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
