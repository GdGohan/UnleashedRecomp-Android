# Bundled Turnip driver

`vulkan.unleashed26_1_wfm_a732.so`

- **What:** a source‑built **Mesa 26.1.4 Turnip** (Adreno open‑source Vulkan
  driver, Vulkan 1.4) with two patches baked in:
  - **0001** — unconditional per‑draw `TU_CMD_FLAG_WAIT_FOR_ME` (`CP_WAIT_FOR_ME`)
    in `tu6_emit_flushes()`, no `TU_DEBUG` gate. Fixes the Adreno 725 "shimmer".
  - **0004** — adds the Adreno **732** chip id to the FD735 device entry.
- **Covers:** Adreno **725 / 732 / 750** (a750 additionally needs the game's
  MSAA set to **None** — that is the Android config default).
- **soname:** `vulkan.adreno.so` (adrenotools‑loadable).

## How it is used
- This exact file is bundled in the APK at
  `android-apk/app/src/main/assets/turnip/` and is extracted to internal storage
  on first launch, then loaded via **libadrenotools**.
- To try a different Turnip build without rebuilding the app, drop a plain `.so`
  into `Android/data/org.libsdl.app/files/driver_import/` on the device.

## ⚠️ TU_DEBUG must stay `none`
The WFM fix is **compiled in**. On a source build like this, setting
`TU_DEBUG=flushall` enables Mesa's *real* full per‑draw cache clean+invalidate —
a huge FPS loss. Leave `tu_debug.txt` at `none` (the default). `flushall` is only
meaningful for the old *binary‑patched* stock drivers, where it gates the patched
mask.

## Provenance / rebuilding
Built in CI from a fork of the Turnip build scripts,
**`SansNope/Banners-Turnip`** (branch `unleashed`); the scripts + patches are in
`../turnip-driver-ci/`. Select the variant with the `VARIANT` env var
(`wfm-a732` produced this file) and the Mesa ref with `MESA_REF` (`26.1`).
Other Adreno 6xx/7xx Turnip builds (e.g. K11MCH1's AdrenoToolsDrivers) also work
if imported via `driver_import/`.
