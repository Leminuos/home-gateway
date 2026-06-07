# Secure Boot — U-Boot Verified Boot

## Tổng quan

Secure Boot trong dự án này hiện triển khai **U-Boot Verified Boot**. dm-verity chưa được triển khai.

Mục tiêu: U-Boot từ chối boot kernel nếu FIT image không có chữ ký RSA hợp lệ, ngăn chặn attacker thay kernel/dtb trên thiết bị.

Tính năng được kiểm soát qua `DISTRO_FEATURES`:
- **Bật** (mặc định cho prod): `secure-boot` có trong `DISTRO_FEATURES`
- **Tắt** (dev mode): thêm `DISTRO_FEATURES:remove = "secure-boot"` vào `local.conf`

---

## Nguyên lý hoạt động

**Luồng boot khi `secure-boot` bật:**

1. MLO load `u-boot.img` vào RAM
2. U-Boot đọc env từ MMC offset 0x260000
3. `bootcmd = run ota_boot` -> `ota_check_rollback` -> `ota_pick_slot` -> `ota_load_kernel` -> `bootm`
4. `ota_load_kernel`: `ext4load mmc 0:${mmc_part} ${kernel_addr_r} /boot/fitImage`
5. `bootm`: parse FIT image -> xác minh chữ ký RSA bằng public key trong `u-boot.dtb`
6. Nếu chữ ký hợp lệ -> boot kernel; nếu sai -> dừng, in lỗi

**Luồng boot khi `secure-boot` tắt:**

Bước 4–6 thay bằng `ext4load zImage + am335x-boneblack.dtb` -> `bootz` — không xác minh chữ ký.

---

## Thành phần kỹ thuật

### FIT Image

FIT (Flattened Image Tree) là format image của U-Boot, đóng gói `kernel` (`zImage`) và `fdt` (`am335x-boneblack.dtb`). Được build bởi `kernel-fitimage.bbclass` trong Yocto. Địa chỉ load/entry cho BBB (Cortex-A8, DDR3 tại `0x80000000`) được set trong machine conf: `UBOOT_LOADADDRESS = "0x80008000"`, `UBOOT_ENTRYPOINT = "0x80008000"`, `UBOOT_DTB_LOADADDRESS = "0x88000000"`. Mỗi slot A/B có `fitImage` riêng tại `/boot/fitImage` trong rootfs của slot đó.

### RSA Key Pair

| File | Nội dung | Lưu trữ |
|---|---|---|
| `${TOPDIR}/keys/dev.key` | Private key RSA2048 — ký FIT | Máy build, **không commit vào repo** |
| `${TOPDIR}/keys/dev.crt` | Public key (X.509) — inject vào u-boot.dtb | Máy build |

Thuật toán: SHA256 + RSA2048 (`FIT_SIGN_ALG = "rsa2048"`, `FIT_HASH_ALG = "sha256"`).

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
```

---

## Giới hạn hiện tại

- **dm-verity chưa triển khai**: rootfs có thể bị sửa trực tiếp trên block device sau khi boot. U-Boot Verified Boot chỉ bảo vệ quá trình khởi động.
- **U-Boot không được bảo vệ ở ROM level**: Kẻ tấn công có thể thay `u-boot.img` trên MMC bằng bản không có public key. Bảo vệ hoàn toàn cần AM335x Secure Boot ở ROM (fusing) — ngoài phạm vi dự án hiện tại.
