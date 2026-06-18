# Secure Boot — Verified Boot + signed SWU

## Tổng quan

Secure Boot trong dự án này gồm **U-Boot Verified Boot** cho boot chain và **SWUpdate signed `.swu`** cho OTA package. dm-verity chưa được triển khai.

Mục tiêu: U-Boot từ chối boot kernel nếu FIT image không có chữ ký RSA hợp lệ và SWUpdate từ chối cài `.swu` nếu descriptor không có chữ ký CMS hợp lệ hoặc payload/script không khớp hash.

Tính năng được kiểm soát qua `DISTRO_FEATURES`:
- **Bật** (mặc định cho prod): `secure-boot` có trong `DISTRO_FEATURES`
- **Tắt** (dev mode): thêm `DISTRO_FEATURES:remove = "secure-boot"` vào `local.conf`

---

## Nguyên lý hoạt động

### Verified Boot

**Luồng boot khi `secure-boot` bật:**

1. MLO load `u-boot.img` vào RAM
2. U-Boot đọc env từ MMC offset 0x260000
3. `bootcmd = run ota_boot` -> `ota_check_rollback` -> `ota_pick_slot` -> `ota_load_kernel` -> `bootm`
4. `ota_load_kernel`: `ext4load mmc 0:${mmc_part} ${kernel_addr_r} /boot/fitImage`
5. `bootm`: parse FIT image -> xác minh chữ ký RSA bằng public key trong `u-boot.dtb`
6. Nếu chữ ký hợp lệ -> boot kernel; nếu sai -> dừng, in lỗi

**Luồng boot khi `secure-boot` tắt:**

Bước 4–6 thay bằng `ext4load zImage + am335x-boneblack.dtb` -> `bootz` — không xác minh chữ ký.

### Signed SWU

**Luồng OTA khi `secure-boot` bật:**

1. `update-image.bb` render `sw-description` với `sha256 = "$swupdate_get_sha256(...)"` cho rootfs image và `switch-slot.sh`
2. `swupdate.bbclass` ký `sw-description` bằng CMS/X.509, dùng `${TOPDIR}/keys/dev.key` và `${TOPDIR}/keys/dev.crt`
3. Image cài SWUpdate có `CONFIG_SIGNED_IMAGES=y`, `CONFIG_SIGALG_CMS=y` và `/etc/swupdate/swupdate.pem`
4. Khi app gọi `swupdate -d -u <url>`, SWUpdate tải `.swu`, verify chữ ký CMS của descriptor bằng `/etc/swupdate/swupdate.pem`, rồi verify hash SHA256 của payload/script
5. Nếu verify OK -> flash slot inactive; nếu chữ ký/hash sai -> abort trước khi ghi slot

**Luồng OTA khi `secure-boot` tắt:**

`SWUPDATE_SIGNING` rỗng, SWUpdate không thêm config signed-images, `swupdate.cfg` không khai báo `public-key-file`; `.swu` không bắt buộc có chữ ký.

---

## Thành phần kỹ thuật

### FIT Image

FIT (Flattened Image Tree) là format image của U-Boot, đóng gói `kernel` (`zImage`) và `fdt` (`am335x-boneblack.dtb`). Được build bởi `kernel-fitimage.bbclass` trong Yocto. Địa chỉ load/entry cho BBB (Cortex-A8, DDR3 tại `0x80000000`) được set trong machine conf: `UBOOT_LOADADDRESS = "0x80008000"`, `UBOOT_ENTRYPOINT = "0x80008000"`, `UBOOT_DTB_LOADADDRESS = "0x88000000"`. Mỗi slot A/B có `fitImage` riêng tại `/boot/fitImage` trong rootfs của slot đó.

### RSA Key Pair

| File | Nội dung | Lưu trữ |
|---|---|---|
| `${TOPDIR}/keys/dev.key` | Private key RSA2048 — ký FIT | Máy build, **không commit vào repo** |
| `${TOPDIR}/keys/dev.crt` | Public key (X.509) — inject vào u-boot.dtb và cài vào `/etc/swupdate/swupdate.pem` | Máy build + target rootfs |

Thuật toán boot image: SHA256 + RSA2048 (`FIT_SIGN_ALG = "rsa2048"`, `FIT_HASH_ALG = "sha256"`). Thuật toán OTA package: CMS/X.509 với cùng key/cert, cộng hash SHA256 cho từng artifact trong `sw-description`.

Khi `FIT_GENERATE_KEYS = "1"` (mặc định khi `secure-boot` bật), Yocto tự sinh key vào `${TOPDIR}/keys/` nếu chưa có. Với production, sinh key một lần, backup private key ở nơi an toàn, set `FIT_GENERATE_KEYS = "0"` để Yocto không tự sinh đè.

### U-Boot với CONFIG_OF_SEPARATE

U-Boot được build với `CONFIG_OF_SEPARATE=y`: device tree của U-Boot (`u-boot.dtb`) là file riêng, không embed vào binary. Điều này cho phép `kernel-fitimage.bbclass` ký FIT xong rồi inject public key vào `u-boot.dtb`, sau đó `uboot-sign.bbclass` ghép `u-boot-nodtb.bin` + `u-boot.dtb` → `u-boot.img` — không cần rebuild U-Boot sau khi sinh key.

### Boot env trong U-Boot

Hai chế độ boot được bake vào U-Boot binary qua patch riêng:

| Mode | Patch | Load command | Boot command |
|---|---|---|---|
| `secure-boot` tắt | `0001-bbb-ota-normal-boot.patch` | `ext4load zImage + dtb` | `bootz` |
| `secure-boot` bật | `0001-bbb-ota-secure-boot.patch` | `ext4load fitImage` | `bootm` |

Cả hai patch đều chứa đầy đủ OTA env (ota_check_rollback, ota_pick_slot, ota_load_kernel, ota_boot) — chỉ khác nhau ở phần load/boot command.

### SWUpdate signing config

`meta-ota/recipes-extended/images/update-image.bb` bật ký `.swu` theo `DISTRO_FEATURES`:

```bitbake
SWUPDATE_SIGNING = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'CMS', '', d)}"
SWUPDATE_CMS_KEY  = "${TOPDIR}/keys/dev.key"
SWUPDATE_CMS_CERT = "${TOPDIR}/keys/dev.crt"
```

`meta-ota/recipes-support/swupdate/swupdate_%.bbappend` chỉ thêm `signed-images.cfg`, copy certificate vào `/etc/swupdate/swupdate.pem`, và render `public-key-file` trong `/etc/swupdate.cfg` khi `secure-boot` bật. Nhờ đó dev mode không bị buộc verify `.swu`, còn production mode dùng cùng root of trust với Verified Boot.

Lưu ý: `signed-images.cfg` phải được append vào `${WORKDIR}/defconfig` trước `do_configure`; chỉ thêm file vào `SRC_URI` mới làm file xuất hiện trong `${WORKDIR}`, chưa làm SWUpdate bật `CONFIG_SIGNED_IMAGES`. Dòng `public-key-file` trong `swupdate.cfg` là runtime config để binary biết dùng certificate nào, nhưng binary chỉ hiểu và thực thi verify chữ ký nếu build-time config đã bật signed-images/CMS.

---

## Cấu hình trong Yocto

Tất cả config tập trung ở `meta-bsp/conf/machine/bbb-home-gateway.conf`:

```bitbake
UBOOT_SIGN_ENABLE     = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '1', '0', d)}"
UBOOT_SIGN_KEYDIR     = "${TOPDIR}/keys"
UBOOT_SIGN_KEYNAME    = "dev"
FIT_GENERATE_KEYS     = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '1', '0', d)}"
FIT_SIGN_ALG          = "rsa2048"
FIT_HASH_ALG          = "sha256"
UBOOT_ENTRYPOINT      = "0x80008000"
UBOOT_LOADADDRESS     = "0x80008000"
UBOOT_DTB_LOADADDRESS = "0x88000000"
KERNEL_IMAGETYPE      = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'fitImage', 'zImage', d)}"
KERNEL_CLASSES:append = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', ' kernel-fitimage', '', d)}"
```

Feature bật mặc định trong `meta-bsp/conf/distro/home-gateway.conf`:

```bitbake
DISTRO_FEATURES:append = " secure-boot"
```

---

## Build flow khi `secure-boot` bật

```
virtual/bootloader:do_compile
    └─ build u-boot-nodtb.bin + u-boot.dtb (CONFIG_OF_SEPARATE=y)
       └─ apply verified-boot.cfg: CONFIG_FIT, CONFIG_FIT_SIGNATURE, CONFIG_RSA…
       └─ apply 0001-bbb-ota-secure-boot.patch

virtual/bootloader:do_deploy
    └─ deploy: u-boot-nodtb.bin, u-boot.dtb → DEPLOY_DIR_IMAGE/

virtual/kernel:do_assemble_fitimage  (kernel-fitimage.bbclass)
    ├─ pack zImage + am335x-boneblack.dtb → fitImage
    ├─ sign fitImage bằng keys/dev.key (SHA256+RSA2048)
    └─ inject public key (dev.crt) vào DEPLOY_DIR_IMAGE/u-boot.dtb

virtual/bootloader:do_concat_dtb  (uboot-sign.bbclass)
    └─ cat u-boot-nodtb.bin + u-boot.dtb → u-boot.img (final, có public key)

core-image-home-gateway:do_image_wic
    └─ MLO + u-boot.img + u-boot-env.raw + rootfs (có /boot/fitImage)

swupdate:do_configure / do_install
    ├─ append signed-images.cfg vào defconfig
    ├─ install keys/dev.crt -> /etc/swupdate/swupdate.pem
    └─ render public-key-file trong /etc/swupdate.cfg

update-image:do_swuimage
    ├─ render sw-description có sha256 cho rootfs ext4.gz + switch-slot.sh
    ├─ sign sw-description bằng CMS với keys/dev.key/dev.crt
    └─ pack sw-description + signature + rootfs ext4.gz + switch-slot.sh -> .swu
```

---

## Giới hạn hiện tại

- **dm-verity chưa triển khai**: rootfs có thể bị sửa trực tiếp trên block device sau khi boot. U-Boot Verified Boot chỉ bảo vệ quá trình khởi động.
- **U-Boot không được bảo vệ ở ROM level**: Kẻ tấn công có thể thay `u-boot.img` trên MMC bằng bản không có public key. Bảo vệ hoàn toàn cần AM335x Secure Boot ở ROM (fusing) — ngoài phạm vi dự án hiện tại.
- **Sign `.swu` chỉ bảo vệ package trước khi cài**: nếu attacker đã có quyền ghi block device trực tiếp trên thiết bị sau khi boot, signed `.swu` không thay thế dm-verity hoặc cơ chế chống sửa rootfs runtime.
