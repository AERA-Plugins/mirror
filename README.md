# AERA Mirror

AERA Mirror displays and controls AERA Recovery on a Linux, macOS, or Windows
computer over USB/ADB. The recovery host owns framebuffer capture and virtual
input; the downloadable plugin only requests explicit start/stop operations.

## Desktop client

Install Android platform-tools, Python 3, Tk, and Pillow. Boot AERA Recovery,
connect USB, then run:

```sh
sudo apt install adb python3-tk python3-pil python3-pil.imagetk
python3 desktop/aera-mirror.py
```

Left-drag acts as touch. Right-click, Escape, or Backspace sends Android Back;
Home sends Android Home. `--fps 1..30` changes the requested frame rate.

## Build plugin

```sh
./source/build-runtime.sh stage
python3 source/pack.py stage build
```

Copy the generated metadata into `plugin.json`, sign the manifest, then package
it with `AERA-plugin-registry/scripts/package_aerap.py`.
