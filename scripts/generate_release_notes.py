#!/usr/bin/env python3
"""
Generate GitHub Release Notes for RO-mesh Core Firmware.
Produces Markdown tables with hardware thumbnails and direct download links.
"""

import sys
import os

def generate(tag):
    img_base = "https://raw.githubusercontent.com/RO-mesh/core-firmware/main/docs/assets/boards"
    dl_base = f"https://github.com/RO-mesh/core-firmware/releases/download/{tag}"

    content = f"""## RO-mesh Core Firmware {tag}

Official pre-release for the Romanian decentralized mesh communications network (`#ro`). Built on a decoupled modular architecture based on MeshCore with Romanian channel plans, dynamic regulatory enforcement, and telemetry beacons.

---

### 📱 Companions (Mobile & Client Devices)

| Board | Model & Role | Primary Downloads | Flashing Method |
| :---: | :--- | :--- | :--- |
| <img src="{img_base}/t1000e.png" width="48" height="48" alt="T1000-E"> | **Seeed SenseCAP T1000-E**<br>Companion (Mobile / GPS / SOS) | • [`t1000e-companion-core-firmware-{tag}-dfu.zip`]({dl_base}/t1000e-companion-core-firmware-{tag}-dfu.zip) ⭐ *(Web Flasher)*<br>• [`t1000e-companion-core-firmware-{tag}.uf2`]({dl_base}/t1000e-companion-core-firmware-{tag}.uf2) *(Drag & Drop)* | **Web Flasher**: Upload `-dfu.zip`<br>**USB Drive**: Double-click button → Drag & Drop `.uf2` |

---

### 📡 Repeaters (Infrastructure & Airborne Relays)

| Board | Model & Role | Primary Downloads | Flashing Method / Notes |
| :---: | :--- | :--- | :--- |
| <img src="{img_base}/heltec-v3.png" width="48" height="48" alt="Heltec V3"> | **Heltec LoRa32 V3**<br>Urban Infrastructure Repeater | • [`heltec-v3-repeater-core-firmware-{tag}-factory.bin`]({dl_base}/heltec-v3-repeater-core-firmware-{tag}-factory.bin) ⭐ *(Factory)*<br>• [`heltec-v3-repeater-core-firmware-{tag}-update.bin`]({dl_base}/heltec-v3-repeater-core-firmware-{tag}-update.bin) *(OTA)* | **Factory**: WebSerial / `esptool` @ `0x0000`<br>**OTA Update**: Web App OTA partition |
| <img src="{img_base}/tbeam-1w.png" width="48" height="48" alt="T-Beam 1W"> | **LilyGo T-Beam 1W**<br>High-Power Backbone Repeater | • [`tbeam-1w-repeater-core-firmware-{tag}-factory.bin`]({dl_base}/tbeam-1w-repeater-core-firmware-{tag}-factory.bin) ⭐ *(Factory)*<br>• [`tbeam-1w-repeater-core-firmware-{tag}-update.bin`]({dl_base}/tbeam-1w-repeater-core-firmware-{tag}-update.bin) *(OTA)* | **Factory**: WebSerial / `esptool` @ `0x0000`<br>**OTA Update**: Web App OTA partition |
| <img src="{img_base}/rak-1w.png" width="48" height="48" alt="RAK 1W"> | **RAK4631 + 1W PA**<br>High-Power Backbone Repeater | • [`rak-1w-repeater-core-firmware-{tag}-dfu.zip`]({dl_base}/rak-1w-repeater-core-firmware-{tag}-dfu.zip) ⭐ *(Web Flasher)*<br>• [`rak-1w-repeater-core-firmware-{tag}.uf2`]({dl_base}/rak-1w-repeater-core-firmware-{tag}.uf2) *(Drag & Drop)* | **Web Flasher**: Upload `-dfu.zip`<br>**USB Drive**: Double-click reset → Drag & Drop `.uf2` |
| <img src="{img_base}/t1000e.png" width="48" height="48" alt="T1000-E"> | **Seeed SenseCAP T1000-E**<br>Airborne Repeater (HAB / Drone) | • [`t1000e-repeater-core-firmware-{tag}-dfu.zip`]({dl_base}/t1000e-repeater-core-firmware-{tag}-dfu.zip) ⭐ *(Web Flasher)*<br>• [`t1000e-repeater-core-firmware-{tag}.uf2`]({dl_base}/t1000e-repeater-core-firmware-{tag}.uf2) *(Drag & Drop)* | **Web Flasher**: Upload `-dfu.zip`<br>**USB Drive**: Double-click button → Drag & Drop `.uf2` |

---

### ℹ️ Flashing Guide & Binary Types

* **`-dfu.zip` (Nordic nRF52840 Web Flashing)**: For browser-based WebSerial flashers (e.g. Meshtastic Web Flasher, Adafruit WebSerial DFU, nRF Connect). Contains `manifest.json` and signed DFU payloads.
  > ⚠️ **Note**: Web flashers require this ZIP format. Uploading `.uf2` directly into a web flasher will produce: `Error: End of central directory not found`.
* **`.uf2` (Nordic nRF52840 Mass Storage Drag & Drop)**: Plug device into your computer via USB, double-click the reset / physical button to mount the virtual USB mass storage drive, and drag & drop the `.uf2` file.
* **`-factory.bin` (ESP32 / ESP32-S3 Web & Serial Flashing)**: Complete flash image including bootloader, partition table, and application. Flash at offset `0x0000` via `esptool.py` or browser WebSerial flashers.
* **`-update.bin` (ESP32 / ESP32-S3 OTA Updates)**: Application-only partition. Used for Over-The-Air (OTA) updates on devices already running RO-mesh.
* **`tools-all-models-...zip`**: Developer archive containing every compiled binary plus Intel `.hex` files for J-Link / OpenOCD and command-line programmers.
"""
    return content

if __name__ == '__main__':
    tag = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("TAG", "v0.1.0-alpha.1")
    out_path = sys.argv[2] if len(sys.argv) > 2 else "release_notes.md"
    notes = generate(tag)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(notes)
    print(f"Generated {out_path} for {tag}")
