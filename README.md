# AERA Mirror

AERA Mirror displays and controls AERA Recovery from any modern browser on the
same Wi-Fi network, or from Linux, macOS, and Windows over USB/ADB.

For Wi-Fi, connect recovery and the computer to the same network, open AERA
Mirror, choose **Start Wi-Fi Mirror**, and enter the displayed `http://PHONE_IP/`
address in any browser. No desktop install is needed. Choose **Start USB Mirror**
instead when using the desktop client below.

## USB one-click launcher

Start **USB Mirror** in AERA, connect the cable, then run the launcher for the
computer:

- Windows: double-click `desktop/start-aera-mirror-usb.cmd`.
- Linux: run `desktop/start-aera-mirror-usb.sh`.
- macOS: double-click `desktop/start-aera-mirror-usb.command`.

The launcher waits for recovery, creates a local ADB tunnel, and opens
`http://127.0.0.1:8080/`. Android platform-tools/ADB must either be installed or
be available in `PATH`; on Windows its `platform-tools` folder may instead be
placed beside the launcher. Python, Pillow, and Tk are not required.

The phone cannot execute a program on a connected computer automatically. That
is deliberately blocked by desktop operating systems. Wi-Fi mode remains the
zero-install option: start it in AERA and open the displayed address.

## Legacy Python client

Install Android platform-tools, Python 3, Tk, and Pillow. Boot AERA Recovery,
connect USB, then run this only if the browser-based launcher is unsuitable:

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
