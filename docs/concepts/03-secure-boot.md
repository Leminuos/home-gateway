# Secure Boot — U-Boot Verified Boot + dm-verity

## Tổng quan

Secure Boot trong dự án này gồm hai lớp: **U-Boot Verified Boot** (xác minh kernel/dtb/initramfs lúc boot) và **dm-verity** (xác minh từng block rootfs khi đọc lúc runtime). Hai lớp nối thành một chuỗi tin cậy duy nhất.

Mục tiêu: U-Boot từ chối boot kernel nếu FIT image không có chữ ký RSA hợp lệ (ngăn thay kernel/dtb), và kernel từ chối đọc bất kỳ block rootfs nào không khớp hash tree (ngăn sửa rootfs trên block device).

## Chain of trust

```
U-Boot public key (trong u-boot.dtb)
   └─ verify chữ ký RSA của FIT image
        └─ FIT chứa: kernel + dtb + initramfs   (cả 3 đều được ký)
             └─ initramfs chứa /etc/verity.env  (ROOT_HASH của rootfs)
                  └─ veritysetup open rootfs với ROOT_HASH
                       └─ mỗi block rootfs đọc ra phải khớp hash tree
```

Vì root hash nằm trong initramfs, mà initramfs nằm trong FIT đã ký, nên không thể sửa rootfs lẫn root hash mà không phá chữ ký FIT. Sửa rootfs -> verity đọc lỗi -> reboot -> rollback. Sửa fitImage -> U-Boot từ chối boot -> rollback.

Tính năng được kiểm soát qua `DISTRO_FEATURES`:
- Bật (mặc định cho prod): `secure-boot` có trong `DISTRO_FEATURES`
- Tắt (dev mode): thêm `DISTRO_FEATURES:remove = "secure-boot"` vào `local.conf`

---

## Nguyên lý hoạt động

**Luồng boot khi `secure-boot` bật:**

1. MLO load `u-boot.img` vào RAM
2. U-Boot đọc env từ MMC offset 0x260000
3. `bootcmd = run ota_boot` -> `ota_check_rollback` -> `ota_pick_slot` (đặt `boot_part`/`root_part` theo slot) -> `ota_load_kernel` -> `bootm`
4. `ota_load_kernel`: `fatload mmc 0:${boot_part} ${kernel_addr_r} fitImage` (fitImage nằm ở boot partition FAT của slot)
5. `bootm`: parse FIT image -> xác minh chữ ký RSA của kernel + dtb + initramfs bằng public key trong `u-boot.dtb`
6. Chữ ký hợp lệ -> boot kernel kèm initramfs; sai -> dừng, in lỗi
7. initramfs `/init`: đọc `root=/dev/mmcblk0p${root_part}` từ cmdline -> `veritysetup open` rootfs với `ROOT_HASH` trong `/etc/verity.env` -> mount `/dev/mapper/rootfs` ro -> `switch_root`. Verity/mount fail -> `reboot -f` -> rollback.

**Luồng boot khi `secure-boot` tắt:**

Bước 4–7 thay bằng `fatload zImage + am335x-boneblack.dtb` từ boot partition -> `bootz`, mount thẳng `root=/dev/mmcblk0p${root_part}` (ext4 thường) — không xác minh chữ ký, không verity, không initramfs.

---

## Thành phần kỹ thuật

### FIT Image

FIT (Flattened Image Tree) là format image của U-Boot, đóng gói `kernel` (`zImage`), `fdt` (`am335x-boneblack.dtb`) và `ramdisk` (initramfs). Được build bởi `kernel-fitimage.bbclass` trong Yocto; khi `INITRAMFS_IMAGE` được set và `INITRAMFS_IMAGE_BUNDLE = "0"`, class tự thêm node `ramdisk` và đưa `"ramdisk"` vào danh sách `sign-images` (ký cùng kernel + fdt). Địa chỉ load/entry cho BBB (Cortex-A8, DDR3 tại `0x80000000`) được set trong machine conf: `UBOOT_LOADADDRESS = "0x80008000"`, `UBOOT_ENTRYPOINT = "0x80008000"`, `UBOOT_DTB_LOADADDRESS = "0x88000000"`. Mỗi slot A/B có `fitImage` riêng nằm ở **boot partition FAT** của slot (bootA/bootB), không còn trong rootfs.

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
| `secure-boot` tắt | `0001-bbb-ota-normal-boot.patch` | `fatload zImage + dtb` (bootA/bootB) | `bootz` |
| `secure-boot` bật | `0001-bbb-ota-secure-boot.patch` | `fatload fitImage` (bootA/bootB) | `bootm` |

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

## Build flow khi bật `secure-boot`

```
virtual/bootloader:do_compile
    └─ apply verified-boot.cfg: CONFIG_FIT, CONFIG_FIT_SIGNATURE, CONFIG_RSA…
    └─ apply 0001-bbb-ota-secure-boot.patch

virtual/bootloader:do_deploy
    └─ deploy: u-boot-nodtb.bin, u-boot.dtb -> DEPLOY_DIR_IMAGE/

core-image-home-gateway:do_rootfs
    └─ core-image-home-gateway-*.ext4.verity
    └─ core-image-home-gateway-*.verity.env

home-gateway-initramfs:do_rootfs
    └─ bake .verity.env -> /etc/verity.env -> cpio.gz

virtual/kernel:do_assemble_fitimage_initramfs  (kernel-fitimage.bbclass)
    ├─ zImage + am335x-boneblack.dtb + initramfs -> fitImage
    ├─ sign kernel + fdt + ramdisk bằng keys/dev.key
    └─ inject public key vào DEPLOY_DIR_IMAGE/u-boot.dtb

virtual/bootloader:do_concat_dtb  (uboot-sign.bbclass)
    └─ cat u-boot-nodtb.bin + u-boot.dtb -> u-boot.img (final, có public key)

home-gateway-disk:do_bootfs -> boot-home-gateway.vfat (chứa fitImage)

home-gateway-disk:do_image_wic
```

Vì rootfs đổi -> root hash đổi -> initramfs rebuild -> fitImage re-sign, taskhash chain của bitbake tự đảm bảo mọi artifact đồng bộ mà không cần `nostamp`.

---

## Giới hạn hiện tại

- **Root hash nằm trong chuỗi đã ký** (initramfs trong FIT) nên dm-verity chống được tamper chủ động rootfs. Chi tiết vì sao chọn cách này: [docs/decisions/02-dm-verity-partition.md](../decisions/02-dm-verity-partition.md).
- **U-Boot không được bảo vệ ở ROM level**: Kẻ tấn công có thể thay `u-boot.img` hoặc `MLO` trên MMC bằng bản không có public key. Bảo vệ hoàn toàn cần AM335x Secure Boot ở ROM (fusing eFuse) — ngoài phạm vi dự án hiện tại.
