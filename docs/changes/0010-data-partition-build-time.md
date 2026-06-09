# 0010 — Build /data partition ext4 tại build time bằng mke2fs

## Tóm tắt

Thêm recipe `data-partition` build sẵn một ext4 image chứa cấu trúc file config (`/data/config/*.json`) ngay tại build time bằng `mke2fs -d`. Image này được nhúng trực tiếp vào disk image qua `--source rawcopy` trong wic, và đi kèm một systemd `.mount` unit để mount partition bằng label khi boot.

---

## Lý do

Ứng dụng cần các file config nằm sẵn trên partition `/data` ngay từ khi xuất xưởng. Build sẵn nội dung partition tại build time có các ưu điểm:

- File config tồn tại ngay khi flash image — app không cần logic "tạo file nếu chưa có" lúc runtime.
- Không phụ thuộc script khởi tạo chạy lúc boot, nên không có race condition giữa việc tạo file và lúc app đọc.
- Toàn bộ nội dung `/data` mặc định nằm trong source control (`files/*.json`), dễ kiểm soát và tái lập.

---

## Cơ chế mke2fs -d

`mke2fs -d <source-dir>` nhận một thư mục trên host và đóng gói toàn bộ nội dung vào ext4 image — tương đương `cp -a` nhưng output là file `.ext4` thay vì filesystem mount. Đây là cách chuẩn để tạo ext4 có nội dung sẵn trong Yocto mà không cần quyền root.

---

## Chi tiết thay đổi

### Recipe `data-partition.bb` (mới)

Recipe nằm tại `meta-ota/recipes-core/data-partition/`.

Khai báo `DEPENDS = "e2fsprogs-native"` để có `mke2fs` ở build host và `inherit systemd deploy` để cài `.mount` unit + deploy ext4 cho wic.

**`do_compile`**: Tạo cấu trúc thư mục staging rồi gọi `mke2fs -d`:

```bash
install -d ${WORKDIR}/data-root/config
install -m 0644 ${WORKDIR}/mqtt.json    ${WORKDIR}/data-root/config/mqtt.json
install -m 0644 ${WORKDIR}/ota.json     ${WORKDIR}/data-root/config/ota.json
install -m 0644 ${WORKDIR}/setting.json ${WORKDIR}/data-root/config/setting.json

mke2fs -t ext4 -L data \
       -d ${WORKDIR}/data-root \
       ${WORKDIR}/data-home-gateway.ext4 \
       ${DATA_PARTITION_SIZE_KiB}    # 131072 KiB = 128 MiB, khớp --fixed-size 128
```

**`do_install`**: Cài `data.mount` vào rootfs:

```bash
install -d ${D}${systemd_system_unitdir}
install -m 0644 ${WORKDIR}/data.mount ${D}${systemd_system_unitdir}/data.mount
```

**`do_deploy`**: Copy ext4 vào `DEPLOYDIR` để wic dùng:

```bash
install -Dm 0644 ${WORKDIR}/data-home-gateway.ext4 ${DEPLOYDIR}/data-home-gateway.ext4
```

### `files/data.mount`

```ini
[Mount]
What=LABEL=data
Where=/data
Type=ext4
Options=defaults
```

Mount bằng label thay vì UUID. UUID được gán ngẫu nhiên mỗi lần `mke2fs` chạy nên không ổn định — label `data` thì cố định.

Lý do cần `.mount` unit riêng: wic với `--source rawcopy` không thêm entry vào `/etc/fstab` (khác `--use-uuid`). Phải có unit systemd để hệ thống biết mount `/data` khi boot. Recipe enable unit qua `SYSTEMD_SERVICE`/`SYSTEMD_AUTO_ENABLE`.

### `files/mqtt.json`, `files/ota.json`, `files/setting.json`

Các file config mặc định được `mke2fs -d` copy vào ext4. Nội dung và ý nghĩa từng file xem tại [0009-runtime-config-to-json.md](0009-runtime-config-to-json.md) (mqtt.json, ota.json) và [0011-app-settings-json.md](0011-app-settings-json.md) (setting.json).

### `meta-ota/wic/bbb-ota.wks`

Dòng partition `/data` dùng `--source rawcopy` để ghi ext4 đã build sẵn vào đúng partition:

```
part /data --source rawcopy --ondisk mmcblk0 --fstype=ext4 --align 4096 --fixed-size 128 --sourceparams="file=data-home-gateway.ext4"
```

Hai lưu ý quan trọng về các option:

- **Giữ `--fstype=ext4`**: option này quyết định MBR partition type ID. Nếu bỏ, wic mặc định `vfat` → ghi type ID FAT (0x0c) → khi boot kernel probe `mmcblk0p3` như FAT và báo `bogus number of reserved sectors` / `Can't find a valid FAT filesystem`, mount `/data` fail. Đặt `ext4` → type ID Linux (0x83).
- **Không dùng `--label`**: với rawcopy, `--label` khiến wic gọi công cụ gán label theo fstype sau khi copy — dễ gây lỗi. Label `data` đã được `mke2fs -L data` bake sẵn trong superblock ext4 nên `data.mount` mount bằng `LABEL=data` vẫn hoạt động.

### `core-image-home-gateway.bbappend`

```diff
 IMAGE_INSTALL:append = " \
     ...
+    data-partition       \
 "

-do_image_wic[depends] += "virtual/bootloader:do_deploy"
+do_image_wic[depends] += "virtual/bootloader:do_deploy data-partition:do_deploy"
```

Thêm `data-partition` vào image để `data.mount` được cài vào rootfs. Phụ thuộc `data-partition:do_deploy` đảm bảo ext4 file có trong `DEPLOYDIR` trước khi wic chạy.

---

## Hành vi khi OTA

Partition `/data` không bị mất dữ liệu khi OTA update. Config user đã chỉnh trên thiết bị sau khi xuất xưởng sẽ giữ nguyên qua mọi lần OTA.

`/data` chỉ bị reset về mặc định khi flash lại toàn bộ disk image.
