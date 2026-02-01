# t4-s3_base-apps

ESP32-S3 applications using the t4-s3_hal_bsp-lvgl base components.

## Setup

**⚠️ Important: Use `--recursive` when cloning!**

This project uses **Git Submodules** to include the [t4-s3_hal_bsp-lvgl](https://github.com/coyotegd/t4-s3_hal_bsp-lvgl) base components. The `--recursive` flag automatically clones the submodule along with this repository.

Clone this repository with submodules:

```bash
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git
```

**Why `--recursive`?**
- The base components (HAL, BSP, LVGL) are stored in a separate repository
- They are linked as a submodule in `components/components/`
- Without `--recursive`, you'll get an empty `components/components/` directory and the build will fail

If you already cloned without `--recursive`, initialize the submodule:

```bash
cd t4-s3_base-apps
git submodule update --init --recursive
```

## Build

```bash
idf.py build
```

## Flash

```bash
idf.py flash monitor
```

## Components

This project uses [t4-s3_hal_bsp-lvgl](https://github.com/coyotegd/t4-s3_hal_bsp-lvgl) as a submodule for base components.
