#!/usr/bin/python3
# Adds PlatformIO post-processing to merge all the ESP flash images into a single image.

import os

Import("env", "projenv")

board_config = env.BoardConfig()
firmware_bin = "${BUILD_DIR}/${PROGNAME}.bin"
merged_bin = os.environ.get("MERGED_BIN_PATH", "${BUILD_DIR}/${PROGNAME}-merged.bin")

def merge_bin_action(source, target, env):
    raw_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        env.subst("$ESP32_APP_OFFSET"),
        source[0].get_abspath(),
    ]
    quoted_images = []
    for item in raw_images:
        val = str(item)
        if " " in val:
            quoted_images.append(f'"{val}"')
        else:
            quoted_images.append(val)

    merged_bin_path = env.subst(merged_bin)
    merge_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$OBJCOPY"',
            "--chip",
            board_config.get("build.mcu", "esp32"),
            "merge_bin",
            "-o",
            f'"{merged_bin_path}"',
            "--flash_mode",
            board_config.get("build.flash_mode", "dio"),
            "--flash_freq",
            "${__get_board_f_flash(__env__)}",
            "--flash_size",
            board_config.get("upload.flash_size", "4MB"),
            *quoted_images,
        ]
    )
    env.Execute(merge_cmd)

env.AddCustomTarget(
    name="mergebin",
    dependencies=firmware_bin,
    actions=merge_bin_action,
    title="Merge binary",
    description="Build combined image",
    always_build=True,
)
