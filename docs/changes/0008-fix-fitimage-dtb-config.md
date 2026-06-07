# 0008 — Fix FIT image dùng sai DTB khi boot với secure-boot

## Tóm tắt

Khi `secure-boot` bật, U-Boot dùng `bootm` để boot FIT image. FIT image chứa nhiều DTB (bone/boneblack/bonegreen), default config là `conf-am335x-bone.dtb` (BeagleBone gốc). U-Boot dùng sai DTB -> kernel không có SPI1/ILI9341 node -> không có `/dev/fb0` -> `home-dashboard` crash.

## Lý do

Vấn đề xuất phát từ 3 điểm:

1. `beaglebone-yocto.conf` set `KERNEL_DEVICETREE = "am335x-bone.dtb am335x-boneblack.dtb am335x-bonegreen.dtb"`. `bbb-home-gateway.conf` không override -> `kernel-fitimage.bbclass` sinh FIT image chứa cả 3 DTB, default là DTB đầu tiên (`am335x-bone.dtb`).

2. Lệnh `bootm ${kernel_addr_r}` trong `0001-bbb-ota-secure-boot.patch` không chỉ định configuration -> U-Boot dùng default config (`conf-am335x-bone.dtb`) thay vì `conf-am335x-boneblack.dtb`.

3. `verified-boot.cfg` có `CONFIG_RSA_VERIFY=y` (không tồn tại trong U-Boot Kconfig) và thiếu `CONFIG_RSA_SOFTWARE_EXP=y` (driver RSA software dùng cho xác minh chữ ký trên BBB, không có hardware crypto).

## Chi tiết thay đổi

### `meta-bsp/conf/machine/bbb-home-gateway.conf`

```diff
+KERNEL_DEVICETREE       = "am335x-boneblack.dtb"
```

FIT image sau fix chỉ còn một configuration `conf-am335x-boneblack.dtb`, đây cũng là default. `bootm ${kernel_addr_r}` sẽ tự động dùng đúng DTB.

---

### `meta-ota/recipes-bsp/u-boot/files/0001-bbb-ota-secure-boot.patch`

```diff
-		"bootm ${kernel_addr_r}\0" \
+		"bootm ${kernel_addr_r}#conf-am335x-boneblack.dtb\0" \
```

Force FIT configuration trong lệnh `bootm` để đề phòng trường hợp FIT image có nhiều config (ví dụ sau này thêm DTB variant).

---

### `meta-bsp/recipes-bsp/u-boot/files/verified-boot.cfg`

```diff
-CONFIG_SHA256=y
-CONFIG_RSA_VERIFY=y
+CONFIG_RSA_SOFTWARE_EXP=y
```

- `CONFIG_RSA_VERIFY=y` không tồn tại trong U-Boot 2022.01 Kconfig — option này bị ignore, gây confusion.
- `CONFIG_RSA_SOFTWARE_EXP=y` là driver thực hiện RSA modular exponentiation bằng software — bắt buộc trên BBB vì không có hardware crypto accelerator. Thiếu option này thì U-Boot không thể xác minh chữ ký RSA của FIT image.
- `CONFIG_SHA256=y` được `CONFIG_FIT_SIGNATURE` tự kéo vào, không cần khai báo tường minh.

## Phân tích ảnh hưởng

**Rebuild cần thiết:**
- `bitbake virtual/kernel`: sinh lại FIT image với `KERNEL_DEVICETREE = "am335x-boneblack.dtb"` — FIT sẽ chỉ có một configuration, load đúng DTB cho BBB.
- `bitbake virtual/bootloader`: apply lại `verified-boot.cfg` với `CONFIG_RSA_SOFTWARE_EXP=y` để RSA verification thực sự hoạt động.

**IMAGE_BOOT_FILES:** `beaglebone-yocto.conf` set `IMAGE_BOOT_FILES` bao gồm `${KERNEL_DEVICETREE}`. Sau khi override, chỉ `am335x-boneblack.dtb` được deploy vào boot partition — không còn `am335x-bone.dtb` và `am335x-bonegreen.dtb` thừa.

**"reboot: system halt" khi poweroff:** Đây là hành vi bình thường của BeagleBone Black. BBB cần PMIC TPS65217 để cắt nguồn hoàn toàn; nếu không có PMIC driver, kernel halt CPU nhưng nguồn không tắt. Message `reboot: system halt` là kernel log chuẩn, không liên quan đến secure-boot.
