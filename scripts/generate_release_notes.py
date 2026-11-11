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

| Board | Model & Role | Primary Download | Flashing Method |
| :---: | :--- | :--- | :--- |
| <img src="{img_base}/t1000e.png" width="48" height="48" alt="T1000-E"> | **Seeed SenseCAP T1000-E**<br>Companion (Mobile / GPS / SOS) | [`t1000e-companion-core-firmware-{tag}.uf2`]({dl_base}/t1000e-companion-core-firmware-{tag}.uf2) ⭐ | **UF2** (Double-click button → Drag & Drop) |

---

### 📡 Repeaters (Infrastructure & Airborne Relays)

| Board | Model & Role | Primary Download | Flashing Method / Notes |
| :---: | :--- | :--- | :--- |
| <img src="{img_base}/heltec-v3.png" width="48" height="48" alt="Heltec V3"> | **Heltec LoRa32 V3**<br>Urban Infrastructure Repeater | [`heltec-v3-repeater-core-firmware-{tag}-factory.bin`]({dl_base}/heltec-v3-repeater-core-firmware-{tag}-factory.bin) ⭐ | **Factory** (`0x0` / WebSerial)<br>• [OTA Update.bin]({dl_base}/heltec-v3-repeater-core-firmware-{tag}-update.bin) |
| <img src="{img_base}/tbeam-1w.png" width="48" height="48" alt="T-Beam 1W"> | **LilyGo T-Beam 1W**<br>High-Power Backbone Repeater | [`tbeam-1w-repeater-core-firmware-{tag}-factory.bin`]({dl_base}/tbeam-1w-repeater-core-firmware-{tag}-factory.bin) ⭐ | **Factory** (`0x0` / WebSerial)<br>• [OTA Update.bin]({dl_base}/tbeam-1w-repeater-core-firmware-{tag}-update.bin) |
| <img src="{img_base}/rak-1w.png" width="48" height="48" alt="RAK 1W"> | **RAK4631 + 1W PA**<br>High-Power Backbone Repeater | [`rak-1w-repeater-core-firmware-{tag}.uf2`]({dl_base}/rak-1w-repeater-core-firmware-{tag}.uf2) ⭐ | **UF2** (Double-click reset → Drag & Drop) |
| <img src="{img_base}/t1000e.png" width="48" height="48" alt="T1000-E"> | **Seeed SenseCAP T1000-E**<br>Airborne Repeater (HAB / Drone) | [`t1000e-repeater-core-firmware-{tag}.uf2`]({dl_base}/t1000e-repeater-core-firmware-{tag}.uf2) ⭐ | **UF2** (Double-click button → Drag & Drop) |

---

### ℹ️ Flashing Guide & Binary Types

* **`-factory.bin` (ESP32 / ESP32-S3)**: Complete flash image including bootloader, partition table, and application. Flash at offset `0x0000` via `esptool.py` or browser WebSerial flashers.
* **`-update.bin` (ESP32 / ESP32-S3)**: Application-only partition. Used for Over-The-Air (OTA) updates on devices already running RO-mesh.
* **`.uf2` (Nordic nRF52840)**: Plug the device into your computer via USB, double-click the reset button to mount the virtual USB mass storage drive, and drag & drop the `.uf2` file.
* **`tools-all-models-...zip`**: Comprehensive developer bundle containing every compiled binary plus Intel `.hex` files and Nordic DFU `.zip` packages for J-Link and command-line programmers.
"""
    return content

if __name__ == '__main__':
    tag = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("TAG", "v0.1.0-alpha.1")
    out_path = sys.argv[2] if len(sys.argv) > 2 else "release_notes.md"
    notes = generate(tag)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(notes)
    print(f"Generated {out_path} for {tag}")
