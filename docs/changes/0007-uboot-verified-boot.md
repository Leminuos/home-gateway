# 0007 — U-Boot Verified Boot (Secure Boot giai đoạn 1)

## Tóm tắt

Triển khai **U-Boot Verified Boot**: kernel + dtb được đóng gói vào FIT image, ký RSA2048/SHA256 tại build time. U-Boot từ chối boot nếu chữ ký không hợp lệ.

Tính năng được kiểm soát qua `DISTRO_FEATURES`: bật mặc định (`secure-boot` trong `DISTRO_FEATURES`), tắt cho dev bằng cách thêm `DISTRO_FEATURES:remove = "secure-boot"` vào `local.conf`.

dm-verity **chưa** triển khai trong thay đổi này.

---

## Files thay đổi

### `meta-bsp` — BSP layer

#### `conf/distro/home-gateway.conf`

```diff
 DISTRO_FEATURES:append = " \
     systemd \
+    secure-boot \
 "
```

Thêm `secure-boot` vào `DISTRO_FEATURES` mặc định. Distro conf chỉ khai báo feature, không cấu hình cụ thể.

---

#### `conf/machine/bbb-home-gateway.conf`

Thêm toàn bộ config cho Verified Boot + FIT image:

| Variable | Giá trị | Ý nghĩa |
|---|---|---|
| `UBOOT_SIGN_ENABLE` | `1` khi secure-boot | Bật signing trong uboot-sign.bbclass |
| `UBOOT_SIGN_KEYDIR` | `${TOPDIR}/keys` | Thư mục chứa key RSA |
| `UBOOT_SIGN_KEYNAME` | `dev` | Tên file key (dev.key / dev.crt) |
| `UBOOT_DTB_BINARY` | `u-boot.dtb` | File DTB của U-Boot (để inject public key) |
| `UBOOT_NODTB_BINARY` | `u-boot-nodtb.bin` | U-Boot binary không có DTB |
| `FIT_GENERATE_KEYS` | `1` khi secure-boot | Tự sinh key nếu chưa có |
| `FIT_SIGN_ALG` | `rsa2048` | Thuật toán sign |
| `FIT_HASH_ALG` | `sha256` | Thuật toán hash |
| `UBOOT_ENTRYPOINT` | `0x80008000` | Kernel entry point |
| `UBOOT_LOADADDRESS` | `0x80008000` | Kernel load address |
| `UBOOT_DTB_LOADADDRESS` | `0x88000000` | FDT load address |
| `KERNEL_IMAGETYPE` | `fitImage` / `zImage` | Loại image theo feature |
| `KERNEL_CLASSES:append` | `kernel-fitimage` | Class build FIT image |

---

#### `recipes-bsp/u-boot/files/verified-boot.cfg`

Kconfig fragment cho U-Boot, chỉ apply khi `secure-boot` bật:

```
CONFIG_FIT=y               # FIT image support
CONFIG_FIT_SIGNATURE=y     # RSA signature verification
CONFIG_FIT_VERBOSE=y       # Log khi verify
CONFIG_RSA=y               # RSA engine
CONFIG_OF_CONTROL=y        # Device tree control trong U-Boot
CONFIG_OF_SEPARATE=y       # DTB của U-Boot tách thành file riêng
CONFIG_DEFAULT_DEVICE_TREE="am335x-boneblack"
CONFIG_CMD_BOOTM=y         # bootm command để boot FIT image
CONFIG_BOOTM_LINUX=y
```

`CONFIG_OF_SEPARATE=y` là config then chốt: cho phép inject public key vào `u-boot.dtb` sau khi compile mà không cần rebuild U-Boot.

---

#### `recipes-bsp/u-boot/u-boot_%.bbappend`

Apply `verified-boot.cfg` vào U-Boot chỉ khi `secure-boot` bật:

```bitbake
SRC_URI:append = " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', \
       'file://verified-boot.cfg', '', d)} \
"
```

---

### `meta-ota` — OTA layer

#### `recipes-bsp/u-boot/files/0001-bbb-ota-normal-boot.patch`

Patch cho dev mode ( tắt `secure-boot`), chứa toàn bộ OTA env:
- Biến trạng thái: `boot_limit`, `active_slot`, `boot_count`, `ustate`
- `ota_check_rollback`, `ota_pick_slot`
- `ota_load_kernel`: `ext4load zImage + am335x-boneblack.dtb`
- `ota_boot`: `bootz ${kernel_addr_r} - ${fdt_addr_r}`

---

#### `recipes-bsp/u-boot/files/0001-bbb-ota-secure-boot.patch`

Patch cho secure-boot mode, chứa toàn bộ OTA env:
- Biến trạng thái: `boot_limit`, `active_slot`, `boot_count`, `ustate`
- `ota_check_rollback`, `ota_pick_slot`
- `ota_load_kernel`: `ext4load fitImage` (1 file chứa cả kernel + dtb đã ký)
- `ota_boot`: `bootm ${kernel_addr_r}` — U-Boot tự verify RSA signature trước khi boot

---

#### `recipes-bsp/u-boot/u-boot_%.bbappend`

Chọn một trong hai patch theo feature, không apply cả hai:

```bitbake
SRC_URI:append = " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', \
       'file://0001-bbb-ota-secure-boot.patch', \
       'file://0001-bbb-ota-normal-boot.patch', d)} \
"
```

---

## Lưu ý vận hành

**Key management:** Key được tự động generate vào `${TOPDIR}/keys/` khi `FIT_GENERATE_KEYS = "1"`. Private key (`dev.key`) không được commit vào git repo. Với production: sinh key một lần, backup an toàn, set `FIT_GENERATE_KEYS = "0"`.

**OTA update với Verified Boot:** Khi build OTA package (`.swu`), `fitImage` bên trong phải được ký bằng cùng private key với `u-boot.dtb` đang chạy trên thiết bị. Nếu thay key -> phải update cả U-Boot mới (có public key mới) trước khi deploy firmware mới.
