# meta-ota — OTA layer cho Smart Home Gateway

## Overview

Yocto layer chứa **toàn bộ phần OTA** của gateway BBB: từ logic A/B + rollback trong U-Boot, partition layout, SWUpdate server nhận file `.swu`, cho đến script confirm boot sau khi update thành công.

---

## Cấu trúc

```
meta-ota/
├── conf/
│   ├── layer.conf                 # khai báo layer
│   └── ota-version.inc            # version metadata (SW_VERSION, HW_REVISION, BOARD_NAME)
│
├── recipes-bsp/                   # bootloader & env — phần U-Boot của OTA
│   ├── u-boot/
│   │   ├── u-boot_%.bbappend      # build u-boot-env.raw, áp patch + cfg
│   │   └── files/
│   │       ├── 0001-add-ota-boot-env.patch  # logic A/B + rollback bake vào built-in env
│   │       ├── 0001-bbb-ota.cfg             # vị trí U-Boot env (offset/size)
│   │       └── u-boot-ota-env.txt           # giá trị env mặc định (boot_limit=3, active_slot=A…)
│   └── libubootenv/
│       ├── libubootenv_%.bbappend
│       └── files/fw_env.config    # cho fw_setenv/fw_printenv (userspace) — phải khớp .cfg
│
├── recipes-core/                  # tích hợp vào image + confirm boot
│   ├── images/core-image-home-gateway.bbappend  # thêm swupdate, pick wks A/B, ghi /etc/hwrevision
│   └── systemd/                                  # ota-confirm-boot: commit slot khi lên multi-user
│       ├── ota-confirm-boot.bb
│       └── files/{ota-confirm-boot.service, ota-confirm-boot.sh}
│
├── recipes-extended/              # đóng gói file .swu
│   └── images/
│       ├── update-image.bb        # recipe sinh .swu (inherit swupdate)
│       └── beaglebone/
│           ├── sw-description.in  # descriptor: install set copy1/copy2, hardware-compat
│           └── switch-slot.sh     # postinst: đổi active_slot, ustate=1, boot_count=0
│
├── recipes-support/               # SWUpdate daemon
│   └── swupdate/
│       ├── swupdate_%.bbappend
│       └── files/
│           ├── defconfig          # build-time: bật webserver, libubootenv, hw-compat
│           ├── swupdate.cfg.in    # runtime: port 8080, document_root, reboot-required
│           └── 09-swupdate-args   # chọn copy1/copy2 theo slot đang chạy
│
└── wic/
    └── bbb-ota.wks                # partition layout A/B + data
```

---

## Thành phần theo thư mục

- **`recipes-bsp/`** — Phần bootloader. Logic A/B (chọn slot, đếm `boot_count`, flip slot khi quá `boot_limit`, `ext4load` kernel/dtb từ rootfs slot active) được bake thẳng vào built-in env của U-Boot qua [`0001-add-ota-boot-env.patch`](recipes-bsp/u-boot/files/0001-add-ota-boot-env.patch); `bootcmd = run ota_boot`. State chung nằm ở **U-Boot env raw** (offset `0x260000`), 2 file [`0001-bbb-ota.cfg`](recipes-bsp/u-boot/files/0001-bbb-ota.cfg) (cho U-Boot) và [`fw_env.config`](recipes-bsp/libubootenv/files/fw_env.config) (cho userspace) **phải cùng offset/size**.
- **`recipes-core/`** — Gắn OTA vào image: [`core-image-home-gateway.bbappend`](recipes-core/images/core-image-home-gateway.bbappend) cài SWUpdate, chọn `bbb-ota.wks`, ghi `/etc/hwrevision` + `/etc/sw-versions`. [`ota-confirm-boot`](recipes-core/systemd/ota-confirm-boot.bb) là systemd service oneshot chạy sau `multi-user.target`: nếu `ustate=1` thì set `ustate=0`, `boot_count=0` → **commit** slot mới.
- **`recipes-extended/`** — Sinh file `.swu`. [`update-image.bb`](recipes-extended/images/update-image.bb) đóng gói rootfs theo [`sw-description.in`](recipes-extended/images/beaglebone/sw-description.in) (2 install set `copy1`/`copy2` cho p2/p3, check `hardware-compatibility`); postinst chạy [`switch-slot.sh`](recipes-extended/images/beaglebone/switch-slot.sh) để đặt env trỏ sang slot vừa flash.
- **`recipes-support/`** — Cấu hình SWUpdate daemon: [`defconfig`](recipes-support/swupdate/files/defconfig) (webserver Mongoose, libubootenv, hw-compat; không sign/encrypt), [`swupdate.cfg.in`](recipes-support/swupdate/files/swupdate.cfg.in) (port 8080, **`reboot-required = false`** — Qt app điều khiển reboot, xem [changes/0005](../docs/changes/0005-ota-pull-model-host-server.md)), [`09-swupdate-args`](recipes-support/swupdate/files/09-swupdate-args) (đang chạy slot nào thì flash slot còn lại).
- **`wic/`** — [`bbb-ota.wks`](wic/bbb-ota.wks) định nghĩa layout: MLO + u-boot + u-boot-env (raw), `rootA` 160M, `rootB` 160M, `data` 128M. Kernel/dtb nằm trong từng rootfs slot (không còn FAT `/boot`); `data` giữ nguyên qua OTA.
- **`conf/`** — [`ota-version.inc`](conf/ota-version.inc) là single source of truth cho `OTA_SW_VERSION` / `OTA_HW_REVISION` / `OTA_BOARD_NAME`, dùng chung bởi image bbappend và `update-image.bb`.

---

## Luồng OTA

> **Pull-model** (từ [changes/0005](../docs/changes/0005-ota-pull-model-host-server.md)): firmware được Qt app trên BBB **chủ động tải** từ server host rồi POST vào SWUpdate daemon qua `localhost:8080` — thay cho việc admin upload thủ công vào web UI. Phần bootloader/A-B dưới đây **không đổi**.

1. Qt app phát hiện version mới (MQTT auto / HTTP manual), tải `.swu` từ host, rồi **POST `http://127.0.0.1:8080/upload`** cho SWUpdate daemon. (Trước đây: admin upload qua web UI tại `http://<board-ip>:8080`.)
2. SWUpdate **flash payload vào slot inactive** (chạy p2 thì flash p3, và ngược lại — chọn bởi `09-swupdate-args`).
3. Postinst [`switch-slot.sh`](recipes-extended/images/beaglebone/switch-slot.sh): đổi `active_slot`, đặt `ustate=1`, reset `boot_count=0`. SWUpdate **không tự reboot** (`reboot-required = false`) — app hiện nút *Reboot now* để user bấm.
4. U-Boot (`ota_boot`): thấy `ustate=1` thì tăng `boot_count`, boot slot mới; nếu `boot_count ≥ boot_limit` (mặc định 3) → flip slot cũ + reset → **rollback tự động**.
5. Lên được `multi-user.target` → [`ota-confirm-boot`](recipes-core/systemd/files/ota-confirm-boot.sh) set `ustate=0`, `boot_count=0` → **commit**.

---

## U-Boot env

| Biến | Ý nghĩa | Ai set |
|---|---|---|
| `active_slot` | `A` / `B` — slot đang boot | `switch-slot.sh` khi flash; U-Boot khi rollback |
| `ustate` | `0` = stable, `1` = vừa upgrade đang test | `switch-slot.sh` set 1; `ota-confirm-boot` set 0 |
| `boot_count` | Số lần đã thử boot slot mới | U-Boot tăng; reset bởi confirm/rollback |
| `boot_limit` | Ngưỡng rollback (mặc định 3) | [`u-boot-ota-env.txt`](recipes-bsp/u-boot/files/u-boot-ota-env.txt) |

```bash
fw_printenv active_slot ustate boot_count boot_limit   # đọc trên board
fw_setenv ustate 0                                     # sửa thủ công (debug)
```

---

## Muốn sửa X → file nào

| Tôi muốn… | Sửa ở đây |
|---|---|
| Đổi kích thước/layout partition | [`wic/bbb-ota.wks`](wic/bbb-ota.wks) |
| Đổi logic rollback (ngưỡng, cách flip, ext4load) | [`0001-add-ota-boot-env.patch`](recipes-bsp/u-boot/files/0001-add-ota-boot-env.patch) |
| Đổi vị trí U-Boot env trên MMC | [`0001-bbb-ota.cfg`](recipes-bsp/u-boot/files/0001-bbb-ota.cfg) **và** [`fw_env.config`](recipes-bsp/libubootenv/files/fw_env.config) (phải khớp!) |
| Đổi giá trị env mặc định | [`u-boot-ota-env.txt`](recipes-bsp/u-boot/files/u-boot-ota-env.txt) |
| Đổi cấu trúc `.swu` (thêm script/image) | [`sw-description.in`](recipes-extended/images/beaglebone/sw-description.in) + [`update-image.bb`](recipes-extended/images/update-image.bb) |
| Đổi cách set env khi flash | [`switch-slot.sh`](recipes-extended/images/beaglebone/switch-slot.sh) |
| Thêm health check khi confirm boot | [`ota-confirm-boot.sh`](recipes-core/systemd/files/ota-confirm-boot.sh) |
| Đổi port web / log SWUpdate | [`swupdate.cfg.in`](recipes-support/swupdate/files/swupdate.cfg.in) |
| Bật signing / encryption | [`defconfig`](recipes-support/swupdate/files/defconfig) |
| Bump version OTA | [`conf/ota-version.inc`](conf/ota-version.inc) |

---

## Liên quan

- Cơ chế A/B chi tiết & decision: [docs/decisions/](../docs/decisions/), [docs/changes/0002-kernel-dtb-ab-via-ext4load.md](../docs/changes/0002-kernel-dtb-ab-via-ext4load.md)
- Build & flash lần đầu: [docs/guides/01-build-and-flash.md](../docs/guides/01-build-and-flash.md)
- Chạy OTA end-to-end: [docs/guides/02-ota-update-workflow.md](../docs/guides/02-ota-update-workflow.md)
