# Overview

Yocto layer chứa board support cho Smart Home Gateway — định nghĩa machine, distro, kernel customization, network,...

---

## Cấu trúc

```
meta-bsp/
├── conf/
│   ├── layer.conf
│   ├── distro/home-gateway.conf           # custom distro
│   └── machine/bbb-home-gateway.conf      # custom machine
│
├── recipes-core/
│   ├── images/
│   │   └── core-image-home-gateway.bb     # image recipe
│   └── busybox/
│       ├── busybox_%.bbappend
│       └── files/busybox.cfg              # busybox config fragments
│
├── recipes-connectivity/
│   └── network/
│       ├── bbb-static-ip.bb               # recipe force static IP
│       └── files/10-eth0.network          # systemd network config
│
├── recipes-kernel/linux/
│   ├── linux-yocto_%.bbappend
│   └── files/
│       ├── 0001-bbb-home-gateway-dts.patch    # patch DTS bật SPI + ILI9341
│       └── bbb_home_gateway.cfg               # kernel config fragment
│
└── recipes-qtapp/home-dashboard/
    ├── home-dashboard_1.0.bb                  # Qt application recipe
    └── files/home-dashboard.service           # systemd unit chạy qt application
```

---

## Machine `bbb-home-gateway`

[bbb-home-gateway](conf/machine/bbb-home-gateway.conf) kế thừa `beaglebone-yocto` (từ poky/meta-yocto-bsp) và:

- Set `APPEND` console = `ttyO0,115200n8`, tắt fbcon (`fbcon=map:off`) để Qt vẽ trực tiếp lên framebuffer của ILI9341 không bị console chiếm.
- Set `MACHINEOVERRIDES` để các recipe có thể override theo `beaglebone-yocto` hoặc `bbb-home-gateway`.

Lưu ý: **partition layout** (A/B + boot + data) không khai báo ở machine.conf mà ở [meta-ota/wic/bbb-ota.wks](../meta-ota/wic/bbb-ota.wks), được pick lên bởi `core-image-home-gateway.bbappend` của meta-ota.

---

## Distro `home-gateway`

[home-gateway](conf/distro/home-gateway.conf) require `poky`:

- Remove các feature không liên quan đến gateway:

    ```
    x11 wayland opengl vulkan bluetooth nfc 3g ppp pcmcia wifi
    alsa pulseaudio gobject-introspection-data pci zeroconf
    debuginfod multiarch
    ```

- Init system: systemd (backfill `sysvinit` bị consider).
- Qt build với backend `linuxfb + tslib + fontconfig` (không X11, không OpenGL).
- `TOOLCHAIN_TARGET_TASK` thêm `mosquitto-dev` để dev có thể cross-compile app dùng GPIO/MQTT.

---

## Image `core-image-home-gateway`

[core-image-home-gateway](recipes-core/images/core-image-home-gateway.bb) require `core-image-base`.

Cài thêm:
- `openssh openssh-sshd` — ssh remote
- `libmosquitto1` — MQTT client lib
- `qtbase ttf-dejavu-sans fontconfig` — Qt5 framebuffer
- `home-dashboard` — Qt app (recipe trong layer này)
- `bbb-static-ip` — static IP config
- `kernel-module-ili9341 kernel-module-drm-mipi-dbi` — driver TFT SPI

Flag `DEVELOPMENT_BUILD ?= "0"` để bật/tắt dev tools (i2c-tools, evtest, systemd-analyze, tslib-tests).

---

## Kernel customization

- `0001-bbb-home-gateway-dts.patch` — patch DTS để khai báo SPI controller + ILI9341 panel + GPIO mapping.
- `bbb_home_gateway.cfg` — kernel config fragment bật `CONFIG_SPI`, `CONFIG_DRM_MIPI_DBI`, `CONFIG_TINYDRM_ILI9341`, …

`COMPATIBLE_MACHINE` được mở rộng cho `bbb-home-gateway` và `KMACHINE` ánh xạ về `beaglebone` để dùng kernel metadata có sẵn của poky.

---

## Qt application `home-dashboard`

Source code app nằm tại [`home-gateway-app/`](../home-gateway-app/) (cùng cấp với `meta-bsp/`, `meta-ota/`). Binary build ra tên `home-gateway-app` (cài tại `/usr/bin/home-gateway-app`).
