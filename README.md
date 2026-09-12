# AERA Mirror

AERA Mirror displays and controls AERA Recovery from any modern browser on the
same Wi-Fi network, or from Linux, macOS, and Windows over USB/ADB.

For Wi-Fi, connect recovery and the computer to the same network, open AERA
Mirror, choose **Start Wi-Fi Mirror**, and enter the displayed `http://PHONE_IP/`
address in any browser. No desktop install is needed. Choose **Start USB Mirror**
instead when using the desktop client below.

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
