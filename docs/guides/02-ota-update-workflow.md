# OTA update workflow end-to-end

Hướng dẫn này mô ta chu trình OTA hoàn chỉnh: build file `.swu` mới, upload qua web UI, quan sát board reboot sang slot mới, và verify update đã commit (hoặc rollback).

---

## 1. Chuẩn bị

- Board đã được flash lần đầu theo và đang chạy ở slot A.
- Build host vẫn còn `build/` directory đã setup ở cùng guide.
- Biết IP của board, check bằng `ip addr` trên board.

Verify state ban đầu trên board:

```bash
# 1. U-Boot env đọc được từ userspace
fw_printenv active_slot ustate boot_count boot_limit
# Kết quả mong đợi:
# active_slot=A
# ustate=0
# boot_count=0
# boot_limit=3

# 2. SWUpdate service đang chạy
systemctl status swupdate
# Active: active (running)

# 3. Web UI accessible
curl -s http://localhost:8080/ | head -5
# HTML của SWUpdate webserver

# 4. ota-confirm-boot đã chạy thành công (oneshot)
systemctl status ota-confirm-boot
# Active: active (exited)

# 5. Version metadata
cat /etc/sw-versions  /etc/hwrevision
```

---

## 2. Build file `.swu`

Trên build host, thay đổi gì đó trong rootfs để biết update có thực sự áp dụng. Ví dụ edit `local.conf` để nâng version:

```bash
echo 'OTA_SW_VERSION = "0.2.0"' >> conf/local.conf
```

Build OTA package:

```bash
bitbake update-image
```

Output ở `tmp/deploy/images/bbb-home-gateway/`:

```
update-image-bbb-home-gateway-0.2.0.swu      ◄── file để upload
update-image-bbb-home-gateway.swu            ◄── symlink tới file trên
```

File `.swu` thực ra là cpio archive chứa `sw-description` + rootfs `ext4.gz`:

```bash
# Trên build host, kiểm tra nội dung .swu
cpio -t < update-image-bbb-home-gateway.swu
# sw-description
# core-image-home-gateway-bbb-home-gateway.ext4.gz
# switch-slot.sh
```

---

## 3. Upload qua web UI

Mở trình duyệt → `http://<board-ip>:8080`.

Giao diện SWUpdate webserver (page mặc định của `swupdate-www` package):

```
+----------------------------------------+
|          SWUpdate Web Server           |
+----------------------------------------+
|                                        |
|  [ Drag .swu file here or click ]      |
|                                        |
|  Progress: [          ] 0%             |
|                                        |
|  Status: Idle                          |
+----------------------------------------+
```

Kéo thả file `update-image-bbb-home-gateway-0.2.0.swu` vào → quan sát progress bar.

---

## 4. Theo dõi quá trình flash

### 4.1. Trên web UI

Progress đi qua các giai đoạn:
- `Uploading` — file đang được POST lên board
- `Verifying` — SWUpdate parse `sw-description`, check hardware-compatibility
- `Downloading` (về phần `.swu` đang được stream từ HTTP request)
- `Installing` — flash raw vào partition đích
- `Success` — postinst đã chạy, sẽ reboot

### 4.2. Trên serial console hoặc SSH

Theo dõi log SWUpdate:

```bash
journalctl -u swupdate -f
```

Sẽ thấy:

```
swupdate[xxx]: SWUPDATE running :  [start_thread] Software updated successfully
swupdate[xxx]: SWUPDATE running :  [installer_thread]  Now executing post-install scripts
swupdate[xxx]: switch-slot.sh: Current slot: A, switching to: B
swupdate[xxx]: SWUPDATE successful !
```

Verify env đã đổi (trước khi board reboot):

```bash
fw_printenv active_slot ustate boot_count
# active_slot=B
# ustate=1
# boot_count=0
```

---

## 5. Quan sát U-Boot bootscript

Trên serial console khi board reboot, U-Boot log:

```
>>> OTA: upgrade boot attempt 1/3
>>> OTA: loading kernel from slot B (mmcblk0p2)
```

`boot_count` được tăng lên 1 (vì `ustate=1`). Kernel load từ `/boot` (chung cho cả 2 slot), `bootargs` set `root=/dev/mmcblk0p2` (slot B).

---

## 6. Sau khi boot vào slot B

Login bình thường, verify slot mới:

```bash
# rootfs đang được mount là p2
mount | grep '/'
# /dev/mmcblk0p2 on / type ext4 ...

# Version mới
cat /etc/sw-versions
# 0.2.0

# Env state — ota-confirm-boot.service đã chạy → ustate=0
fw_printenv active_slot ustate boot_count
# active_slot=B
# ustate=0           ◄── đã commit
# boot_count=0
```

Verify service confirm đã chạy thành công:

```bash
journalctl -u ota-confirm-boot
# OTA: Boot confirmed successfully, committing slot
```

->  Lần OTA tiếp theo sẽ flash vào p1 (slot A cũ).

---

## 7. Test rollback

Để verify rollback hoạt động, tạo một update fail có chủ ý.

### 7.1. Cách đơn giản: kernel panic

Edit `local.conf`:

```bitbake
OTA_SW_VERSION = "0.3.0-broken"
```

Thêm một image-postprocess command để làm hỏng init binary:

```bitbake
broken_init() {
    rm -f ${IMAGE_ROOTFS}/sbin/init
}
ROOTFS_POSTPROCESS_COMMAND:append:pn-core-image-home-gateway = " broken_init; "
```

Build `update-image`, upload, đợi reboot.

### 7.2. Quan sát rollback

Trên serial console:

```
>>> OTA: upgrade boot attempt 1/3
>>> OTA: loading kernel from slot A (mmcblk0p1)
[kernel load OK]
Kernel panic - not syncing: No working init found.
[reboot sau 10s do panic=10]

>>> OTA: upgrade boot attempt 2/3
[panic again, reboot]

>>> OTA: upgrade boot attempt 3/3
[panic again, reboot]

>>> OTA: Boot failed 3 times -> rolling back
>>> OTA: loading kernel from slot B (mmcblk0p2)     ◄── slot cũ, đã commit ở section 6
[boot bình thường]
```

Sau rollback, login vào slot B:

```bash
fw_printenv active_slot ustate boot_count
# active_slot=B     ◄── đã flip về slot cũ
# ustate=0
# boot_count=0

cat /etc/sw-versions
# 0.2.0             ◄── version cũ, không phải 0.3.0-broken
```

-> Rollback thành công, version vẫn ở `0.2.0`.

---

## 8. Debug khi update fail

### 8.1. SWUpdate báo lỗi "hardware mismatch"

```
[ERROR] : SWUPDATE failed [0] HW compatibility not verified
```

Nghĩa là `hardware-compatibility` trong sw-description không khớp `/etc/hwrevision` trên board. Check:

```bash
cat /etc/hwrevision
# homegateway 1.0
```

Phải khớp dòng `hardware-compatibility: [ "1.0" ]` trong [sw-description.in](../../meta-ota/recipes-extended/images/beaglebone/sw-description.in).

### 8.2. Update thành công nhưng board không reboot

Check `reboot-required = true` trong [swupdate.cfg](../../meta-ota/recipes-support/swupdate/files/swupdate.cfg.in). Hoặc reboot thủ công:

```bash
fw_printenv ustate    # confirm = 1 (đã sẵn sàng)
reboot
```

### 8.3. Sau update, vẫn boot vào slot cũ

Check env có thực sự đổi không:

```bash
fw_printenv active_slot ustate
```

Nếu `active_slot` chưa đổi -> `switch-slot.sh` đã không chạy -> kiểm tra `journalctl -u swupdate` xem postinst có error.

### 8.4. Board lặp lại retry forever, không rollback

`boot_count` không tăng được — thường là vì `saveenv` fail (env partition write-protect / corrupt). Check trong U-Boot console:

```
saveenv
## Error: Environment version is wrong
```

Cần re-flash `u-boot-env.raw` từ build host:

```bash
sudo dd if=u-boot-env.raw of=/dev/sdX seek=$((0x260000 / 512)) bs=512
```

(với SD đã rút ra cắm vào host).

---

## 9. Tự động hóa upload (CLI thay vì web UI)

SWUpdate webserver chấp nhận POST file trực tiếp. Dùng `curl` trên build host:

```bash
curl -F "file=@update-image-bbb-home-gateway.swu" \
     http://<board-ip>:8080/upload
```

Hoặc một script wrapper:

```bash
#!/bin/bash
set -eu
BOARD_IP="${1:-192.168.1.100}"
SWU_FILE="${2:-update-image-bbb-home-gateway.swu}"

echo "Uploading ${SWU_FILE} to ${BOARD_IP}..."
curl -F "file=@${SWU_FILE}" "http://${BOARD_IP}:8080/upload"
echo "Waiting for board to reboot..."
sleep 5
until ping -c 1 -W 1 "${BOARD_IP}" >/dev/null 2>&1; do
    sleep 2
done
echo "Board is back online."
```