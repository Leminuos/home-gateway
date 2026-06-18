# OTA update workflow end-to-end

Hướng dẫn này mô tả chu trình OTA hoàn chỉnh: build file `.swu` mới, cài bằng SWUpdate downloader, quan sát board reboot sang slot mới, và verify update đã commit hoặc rollback. Luồng UI/app hiện tại dùng pull-model qua host server; xem thêm [08-ota-host-server-va-app-update.md](08-ota-host-server-va-app-update.md).

---

## 1. Chuẩn bị

- Board đã được flash lần đầu và đang chạy ở slot A.
- Build host vẫn còn `build/` directory đã setup.
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

# 2. SWUpdate package + downloader script đã có
which swupdate
which 09-swupdate-args

# 3. ota-confirm-boot đã chạy thành công (oneshot)
systemctl status ota-confirm-boot
# Active: active (exited)

# 4. Version metadata
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
update-image-bbb-home-gateway-0.2.0.swu      ◄── file để cài
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

Khi `secure-boot` bật, `sw-description` được ký CMS/X.509 và descriptor có hash SHA256 cho rootfs image + `switch-slot.sh`. Có thể kiểm tra package có chữ ký bằng:

```bash
cpio -t < update-image-bbb-home-gateway.swu | grep -E 'sw-description|sig'
```

---

## 3. Cài qua SWUpdate downloader

Đưa file `.swu` lên một HTTP server mà BBB truy cập được. Với luồng app hiện tại, `tools/ota-server/ota_server.py` phục vụ file trong thư mục `release/` ở port 8000 và publish manifest cho app.

Ví dụ test nhanh từ host:

```bash
cd tools/ota-server
python3 ota_server.py
cp /path/to/tmp/deploy/images/bbb-home-gateway/update-image-bbb-home-gateway-0.2.0.swu ../../release/
```

Trên BBB, chạy downloader một lần với URL `.swu`:

```bash
09-swupdate-args http://192.168.137.1:8000/release/update-image-bbb-home-gateway-0.2.0.swu
```

---

## 4. Theo dõi quá trình flash

### 4.1. Trên app hoặc progress socket

Progress đi qua các giai đoạn:
- `Downloading` — SWUpdate tự tải `.swu` từ URL
- `Verifying` — SWUpdate parse `sw-description`, check hardware-compatibility, verify CMS signature và hash nếu `secure-boot` bật
- `Installing` — flash raw vào partition đích
- `Success` — postinst đã chạy, app hoặc người dùng quyết định reboot

### 4.2. Trên serial console hoặc SSH

Theo dõi log SWUpdate:

```bash
journalctl -t swupdate -f
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

Vì `reboot-required = false`, SWUpdate không tự reboot. Reboot bằng nút **Reboot now** của app hoặc chạy:

```bash
reboot
```

---

## 5. Quan sát U-Boot bootscript

Trên serial console khi board reboot, U-Boot log:

```
>>> OTA: upgrade boot attempt 1/3
>>> OTA: loading kernel from slot B (mmcblk0p2)
```

`boot_count` được tăng lên 1 (vì `ustate=1`). U-Boot load kernel/dtb từ `/boot` trong rootfs của slot active, `bootargs` set `root=/dev/mmcblk0p2` (slot B).

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

Build `update-image`, cài qua downloader, rồi reboot để boot thử slot mới.

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

### 8.2. SWUpdate báo lỗi signature hoặc hash

Khi `secure-boot` bật, `.swu` phải được ký bằng private key tương ứng với certificate trong `/etc/swupdate/swupdate.pem`, và payload phải khớp SHA256 trong `sw-description`. Check:

```bash
journalctl -t swupdate -n 100
grep public-key-file /etc/swupdate.cfg
ls -l /etc/swupdate/swupdate.pem
```

Nếu file `.swu` bị copy lỗi, bị sửa sau khi build, hoặc build bằng key khác với image đang chạy trên board, SWUpdate sẽ abort trước khi ghi slot inactive.

### 8.3. Update thành công nhưng board không reboot

Thiết kế hiện tại đặt `reboot-required = false` trong [swupdate.cfg](../../meta-ota/recipes-support/swupdate/files/swupdate.cfg.in), để Qt app điều khiển thời điểm reboot. Có thể reboot thủ công:

```bash
fw_printenv ustate    # ustate=1 nghĩa là update đã sẵn sàng boot thử
reboot
```

### 8.4. Sau update, vẫn boot vào slot cũ

Check env có thực sự đổi không:

```bash
fw_printenv active_slot ustate
```

Nếu `active_slot` chưa đổi -> `switch-slot.sh` đã không chạy -> kiểm tra `journalctl -t swupdate` xem postinst có error.

### 8.5. Board lặp lại retry forever, không rollback

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

## 9. Cài bằng CLI

Luồng hiện tại không cần web UI upload. Chỉ cần URL HTTP tới `.swu`:

```bash
09-swupdate-args http://<host-ip>:8000/release/update-image-bbb-home-gateway.swu
```

Hoặc từ Qt app, bấm **Update now** để app gọi cùng downloader script và theo dõi progress qua `/tmp/swupdateprog`.
