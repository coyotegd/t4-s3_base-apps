# t4-s3_base-apps

ESP32-S3 applications using the t4-s3_hal_bsp-lvgl base components.

## Setup

Clone this repository with submodules:

```bash
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git
```

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
