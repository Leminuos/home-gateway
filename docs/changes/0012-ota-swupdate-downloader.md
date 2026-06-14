# 0012 — OTA chuyển sang SWUpdate downloader (đưa URL trực tiếp cho SWUpdate)

## Tóm tắt

Thay đổi cách cài firmware: thay vì Qt app tải file `.swu` về `/tmp` rồi POST (upload) lên webserver của SWUpdate daemon, **app đưa thẳng URL của `.swu` cho SWUpdate để SWUpdate tự `curl` tải về rồi flash**. Hệ quả: tắt hẳn SWUpdate webserver daemon thường trực (port 8080), và app gọi SWUpdate ở chế độ **downloader** (one-shot) qua script `09-swupdate-args`. Phần phát hiện version (MQTT/HTTP manifest) và cơ chế A/B + rollback **không đổi**.

## Lý do

Luồng cũ (xem [changes/0005](0005-ota-pull-model-host-server.md)) đi 2 chặng mạng: app tải `.swu` từ host về `/tmp` (chặng 1), rồi upload chính file đó qua HTTP lên `127.0.0.1:8080` cho SWUpdate (chặng 2). Việc này tốn thêm dung lượng `/tmp` (RAM/tmpfs), ghi–đọc thừa một lần, và app phải tự quản lý vòng đời file tạm. SWUpdate vốn có sẵn module **downloader** (curl) để tự tải `.swu` từ URL — dùng nó bỏ được toàn bộ chặng tải + file tạm + upload, đồng thời tận dụng luôn cơ chế retry/timeout và hash-verify của SWUpdate.

## Vấn đề kỹ thuật: vì sao phải tắt webserver daemon

SWUpdate khi khởi động tạo các socket IPC cố định trong `/tmp` (`/tmp/swupdateprog` để phát tiến độ, `/tmp/sockinstctrl` để điều khiển). **Hai tiến trình SWUpdate không thể chạy song song** vì sẽ giành cùng các socket này.

- Luồng cũ: SWUpdate chạy **nền liên tục** ở chế độ webserver (systemd `swupdate.service`), giữ socket; app chỉ là HTTP client POST file lên. Một tiến trình SWUpdate duy nhất.
- Luồng mới: chế độ downloader là một tiến trình SWUpdate **one-shot** (`swupdate -d -u <url> …`) — chạy xong tự thoát. Nếu webserver daemon vẫn chạy nền, tiến trình one-shot sẽ đụng socket IPC với nó.

Vì pull-model, webserver chỉ phục vụ đúng một consumer là Qt app. Khi đã chuyển sang downloader, webserver không còn ai dùng → **tắt hẳn daemon thường trực** (phương án A) là sạch nhất: mỗi lần update app tự spawn một tiến trình SWUpdate one-shot, không còn xung đột socket, không còn port 8080. Đánh đổi: bỏ luôn trang web upload `.swu` thủ công ở `http://<board-ip>:8080` (không còn dùng trong pull-model).

## Chi tiết thay đổi

### `meta-ota/recipes-support/swupdate/files/09-swupdate-args`

Trước đây file này được `swupdate.sh` (của daemon) **source** để set biến `SWUPDATE_ARGS` / `SWUPDATE_WEBSERVER_ARGS`. Nay đổi thành **script thực thi** nhận URL và gọi SWUpdate downloader, vẫn giữ logic chọn slot theo slot đang chạy:

```diff
-# Chọn install set theo slot đang chạy:
-rootfs=$(swupdate -g)
-
-if [ "$rootfs" = '/dev/mmcblk0p1' ]; then
-	selection="-e stable,copy2"
-else
-	selection="-e stable,copy1"
-fi
-
-SWUPDATE_ARGS="-H @BOARD_NAME@:@HW_REVISION@ ${selection} -f /etc/swupdate.cfg"
-SWUPDATE_WEBSERVER_ARGS="-document_root /www -port 8080"
+#!/bin/sh
+# Usage: 09-swupdate-args <swu-url>
+set -e
+url="$1"
+[ -n "$url" ] || { echo "usage: $(basename "$0") <swu-url>" >&2; exit 2; }
+
+rootfs=$(swupdate -g)
+if [ "$rootfs" = '/dev/mmcblk0p1' ]; then
+    selection="-e stable,copy2"
+else
+    selection="-e stable,copy1"
+fi
+
+exec swupdate -d "-u $url" -H @BOARD_NAME@:@HW_REVISION@ $selection -f /etc/swupdate.cfg
```

`-d "-u <url>"` là tham số của module downloader; `-e stable,copyN` vẫn chọn install set như cũ; retry/timeout đọc từ `/etc/swupdate.cfg`.

### `meta-ota/recipes-support/swupdate/swupdate_%.bbappend`

- Tắt auto-start daemon: thêm `SYSTEMD_AUTO_ENABLE:${PN} = "disable"` (áp cho cả `swupdate.service` lẫn `swupdate.socket`).
- Cài `09-swupdate-args` thành **script thực thi** ở `${bindir}/09-swupdate-args` (mode 0755) thay vì file config 0644 trong `conf.d`; vẫn sed thay `@BOARD_NAME@`/`@HW_REVISION@` lúc build.

```diff
-SYSTEMD... (không có)
+SYSTEMD_AUTO_ENABLE:${PN} = "disable"

-    install -d ${D}${libdir}/swupdate/conf.d
-    install -m 0644 ${WORKDIR}/09-swupdate-args ${D}${libdir}/swupdate/conf.d/09-swupdate-args
+    install -d ${D}${bindir}
+    install -m 0755 ${WORKDIR}/09-swupdate-args ${D}${bindir}/09-swupdate-args
```

### `meta-ota/recipes-support/swupdate/files/swupdate.cfg.in`

Bỏ block `webserver { … port 8080 … }` (không còn webserver). Giữ `globals` (`reboot-required = false`), `download` (retries/timeout — nay được downloader dùng) và `identify`.

### `home-gateway-app/src/ota/OtaConfig.h`

- Bỏ `swupdateUploadUrl()` (URL POST webserver) và `downloadPath()` (file tạm `/tmp`).
- Thêm `swupdateDownloadTool()` → `/usr/bin/09-swupdate-args` (override qua env `OTA_SWUPDATE_TOOL`).

### `home-gateway-app/src/ota/SwupdateProgressClient.{h,cpp}`

- Tách signal `progress(int)` cũ thành **`downloadProgress(int)`** (status `SWU_DOWNLOAD`, lấy `dwl_percent`) và **`flashProgress(int)`** (status `SWU_RUN`/`SWU_PROGRESS`, lấy `cur_percent`) → khớp 2 bước Download/Flash của UI.
- Thêm **retry kết nối**: socket `/tmp/swupdateprog` nay do tiến trình one-shot tạo (có độ trễ), nên `start()` thử lại tối đa ~10s (40 × 250ms) trước khi báo `connectionError`.
- **Xử lý handshake PROGRESS_API v2 (SWUpdate 2025.05)**: khi client kết nối, server gửi `struct progress_connect_ack` (8 byte, magic `"ACK"`) **trước** stream `progress_msg`. Client cũ coi toàn bộ stream là `progress_msg` nên 8 byte ack làm **lệch toàn bộ** struct về sau → mọi message thành rác → UI không có %. Nay `onReadyRead()` đọc/bỏ 8 byte ack trước khi parse. (Luồng cũ không lộ lỗi này vì % do `uploadProgress` HTTP lái và hoàn tất do `ustate poll`, không thực sự dùng % từ socket.) Đồng thời sửa comment ABI: bản build thực tế là **2025.05**, không phải 2022.05.

### `home-gateway-app/src/ota/OtaManager.{h,cpp}`

- Bỏ toàn bộ phần tải HTTP + upload multipart: `mDownloadReply`, `mUploadReply`, `mDownloadFile`, `onDownloadFinished()`, `startInstall()`, `onUploadProgress()`, `onUploadFinished()`.
- Bỏ `mUstatePoller` + `pollUstate()` (fallback phát hiện hoàn tất qua U-Boot env) — nay kết quả lấy trực tiếp từ **exit code** của tiến trình one-shot.
- Thêm `QProcess mInstallProc`. `confirmUpdate()` giờ: `emit phaseDownload()` → `mSwu.start()` → `mInstallProc.start("/usr/bin/09-swupdate-args", {mLatest.url})`.
- `onFlashProgress()`: message flash đầu tiên → `emit phaseFlash()`, kẹp % ≤ 99. SWUpdate báo `cur_percent` **theo từng step** (ghi rootfs → chạy `switch-slot.sh`…), reset về 0 mỗi step mới → thanh flash bị tụt về 0%. Khắc phục bằng `mFlashPctMax`: chỉ cho thanh **tăng, không lùi** (giữ ở mức cao nhất tới khi `concludeSuccess` đặt 100%).
- `onInstallFinished(exitCode, status)`: exit 0 → `concludeSuccess()`; ngược lại → `concludeFailure()` kèm dòng cuối stderr của swupdate.
- `cancel()`: `mInstallProc.kill()` (UI chỉ cho Cancel ở phase download).

## Phân tích ảnh hưởng

- **Bỏ phụ thuộc webserver/port 8080**: không còn daemon SWUpdate chạy nền; RAM thấp hơn một chút, không còn cổng 8080 mở. Mất khả năng upload `.swu` thủ công qua web UI — chấp nhận theo pull-model.
- **Build SWUpdate không đổi**: `CONFIG_DOWNLOAD=y` + `CONFIG_CHANNEL_CURL=y` đã bật sẵn trong `defconfig`, nên không cần build lại feature; chỉ đổi cách chạy.
- **Tiến độ Download nay là số thật từ SWUpdate** (`dwl_percent`) thay vì từ `QNetworkReply` của app; tiến độ Flash vẫn từ socket. Nếu socket lỡ không kết nối được, update vẫn kết luận đúng qua exit code (chỉ thiếu thanh %).
- **Hash-verify**: downloader + `CONFIG_HASH_VERIFY=y` sẽ verify `sha256` nếu `sw-description` khai báo — chặt hơn luồng cũ (vốn đã tắt verify SHA phía app).
- **Bảo mật mạng**: SWUpdate tải `.swu` qua HTTP (chưa bật `CONFIG_DOWNLOAD_SSL`) từ host server — giữ nguyên mức như trước (app cũng tải qua HTTP).
- **Tương thích**: URL `.swu` vẫn lấy từ `manifest.url` như cũ; logic chọn slot A/B, `switch-slot.sh`, `ota-confirm-boot`, U-Boot env đều không đổi.
