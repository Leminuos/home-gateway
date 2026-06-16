# 0015 — Lưu system log persistent

## Tóm tắt

Cấu hình systemd journald lưu persistent trên partition `/data` thay vì để ở tmpfs volatile. Nhờ đó toàn bộ log hệ thống tồn tại qua reboot và A/B rollback, đọc lại được bằng `journalctl` khi điều tra sự cố. Không cần viết logger riêng trong app.

Kèm theo một thay đổi nhỏ phía app để journal không bị nhiễu: cắt log noise của `home-dashboard` ở production bằng build-time `QT_NO_DEBUG_OUTPUT`.

## Lý do

- Diagnostic log trước đây chỉ là `qDebug/qWarning` đi vào systemd journal, mà dưới read-only-rootfs `/var/log` là tmpfs volatile -> journal mất khi reboot/rollback OTA, đúng lúc cần điều tra sự cố hiện trường nhất. Thay vì viết logger riêng trùng lặp với journald thì tận dụng journald và làm nó persistent trên `/data`.
- journald chỉ có 2 store/instance và batch ghi xuống disk theo `SyncIntervalSec`, nên một tier persistent đã đạt mục tiêu "critical bền ngay, log thường gom rồi mới xuống /data" mà không cần namespace/tier RAM.
- Việc cắt noise chỉ là phụ trợ: noise thật nằm theo *priority* (qDebug ồn của app), nên cắt tại nguồn ở app là đúng chỗ, giúp journal trên `/data` gọn và đỡ wear eMMC.

## Chi tiết thay đổi

### `meta-ota/recipes-core/journald-persistent/` (mới)

Recipe `journald-persistent.bb` cài 3 file.

`files/10-persistent.conf` nằm trong folder `/etc/systemd/journald.conf.d/`:

```ini
[Journal]
Storage=persistent
SyncIntervalSec=300
SystemMaxUse=20M
SystemKeepFree=15M
SystemMaxFileSize=4M
SystemMaxFiles=5
MaxRetentionSec=1month
MaxFileSec=1week
```

`files/journald-data.conf` nằm trong folder `/etc/tmpfiles.d/` (chỉ set quyền thư mục đích):

```
d /data/journal 2755 root systemd-journal -
```

`files/journald-data-flush.service` (oneshot, enable sẵn) — tạo symlink `/var/log/journal -> /data/journal` rồi flush, chạy **sau khi `/data` đã mount**:

```ini
[Unit]
Description=Redirect and flush systemd journal to /data
After=data.mount
Requires=data.mount

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mkdir -p /data/journal
ExecStart=/bin/ln -sfn /data/journal /var/log/journal
ExecStart=/bin/journalctl --flush

[Install]
WantedBy=multi-user.target
```

- `Storage=persistent`: journald lưu log vào `/var/log/journal` (đã trỏ sang `/data/journal`).
- `SyncIntervalSec=300`: gom ghi 5 phút/lần để giảm wear eMMC; message priority CRIT+ vẫn được fsync ngay.
- Nhóm `SystemMaxUse/SystemKeepFree/SystemMaxFileSize/SystemMaxFiles/MaxRetentionSec/MaxFileSec`: rotation native của journald — tự đóng file archive khi chạm ngưỡng size/time và xoá file cũ nhất khi vượt tổng dung lượng/số file/độ trống/tuổi. Không cần logrotate ngoài.

**Vì sao cần service flush riêng (không dùng symlink qua tmpfiles):** `/var/log` trên image là tmpfs volatile (symlink `-> volatile/log`), nên symlink journal phải tạo lại mỗi boot. Quan trọng hơn, ở boot thực tế `systemd-journal-flush` chạy rất sớm (~8.5s) — **trước** khi `/data` được mount (~14s) và trước khi `systemd-tmpfiles-setup` tạo được symlink (flush có `Before=systemd-tmpfiles-setup`). Hệ quả: lúc flush không có đích persistent, journald kẹt ghi ở `/run` (RAM) cả phiên và **không tự flush lại** → log mất khi reboot. Giải pháp: một oneshot `After=data.mount` tự tạo symlink rồi `journalctl --flush` đúng lúc `/data` đã sẵn sàng, buộc journald migrate runtime -> `/data`. (Đã kiểm chứng trên board: sau reboot, journal của boot hiện tại nằm ở `/data/journal/<machine-id>/system.journal`, `/run/log/journal` rỗng.)

### `meta-ota/recipes-core/data-partition/data-partition.bb`

```diff
do_compile() {
+    install -d ${WORKDIR}/data-root/journal
```

Tạo sẵn `/data/journal` trong image partition để tránh race lần đầu boot.

### `meta-ota/recipes-core/images/core-image-home-gateway.bbappend`

```diff
     data-partition       \
+    journald-persistent  \
```

## Thay đổi kèm theo

Để journal persistent trên `/data` không bị nhiễu bởi qDebug ồn của app, strip qDebug ngay lúc biên dịch bản production.

### `home-gateway-app/CMakeLists.txt`

```diff
+option(PRODUCTION_BUILD "Strip qDebug() output for production" OFF)
+if(PRODUCTION_BUILD)
+  target_compile_definitions(home-gateway-app PRIVATE QT_NO_DEBUG_OUTPUT)
+endif()
```

`QT_NO_DEBUG_OUTPUT` biến `qDebug()` thành no-op lúc biên dịch; `qWarning/qCritical` giữ nguyên.

### `meta-bsp/recipes-qtapp/home-dashboard/home-dashboard_1.0.bb`

```diff
+EXTRA_OECMAKE += "${@'-DPRODUCTION_BUILD=ON' if d.getVar('DEVELOPMENT_BUILD') != '1' else '-DPRODUCTION_BUILD=OFF'}"
```

Nối với cờ global `DEVELOPMENT_BUILD` đặt trong `local.conf`: production (`0`) strip qDebug, dev (`1`) giữ debug đầy đủ.

## Phân tích ảnh hưởng

- **journald / /data**: log nay nằm trên `/data`, sống qua reboot và A/B rollback. Cap 20M journal + tối thiểu chừa 15M trống, an toàn với partition 128 MiB (dùng chung config + sensors.csv). Wear eMMC tăng nhẹ so với volatile nhưng được giảm bằng `SyncIntervalSec`.
- **Tương thích read-only-rootfs**: hướng journald-trên-/data chạy đúng cả khi read-only-rootfs bật hay tắt (hiện đang comment trong bbappend), và là điều kiện cần để bật read-only-rootfs mà vẫn giữ được log.
- **Production vs dev**: chỉ ảnh hưởng output qDebug; mọi `qWarning/qCritical` (sensor init fail, MQTT error, OTA fail) vẫn được ghi journal ở mọi build.
- **Đọc log hiện trường**: `journalctl -b -1` (boot trước), `journalctl -u swupdate`, `journalctl -p warning`, `journalctl --disk-usage`.
