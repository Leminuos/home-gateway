# OTA pull-model: chạy server host + update từ Qt app

Hướng dẫn quá trình OTA mới end-to-end: dựng server firmware trên host, BBB phát hiện bản mới và cài qua giao diện Qt theo 2 chế độ **Auto** / **Manual**.

> Cơ chế A/B + rollback không đổi; xem [concepts/01-ota-ab-architecture.md](../concepts/01-ota-ab-architecture.md).
> Bối cảnh thay đổi: [changes/0005-ota-pull-model-host-server.md](../changes/0005-ota-pull-model-host-server.md).

---

## 1. Tổng quan luồng

```
HOST (192.168.137.1)                          BBB
+--------------------+                      +--------------------------------------+
| ota_server.py      | -- manifest.json --> | Manual: nhấn nút Check for updates   |
|  HTTP :8000        |   (GET)              |                                      |
|   /manifest.json   | -- ota/latest -----> | Auto: xuất hiện popup khi nhận MQTT  |
|   /release/*.swu   |   (MQTT retained)    |                                      |
|                    | -- .swu (HTTP)  ---> | Tải về /tmp -> POST cho SWUpdate     |
+--------------------+                      | daemon flash                         |
                                            +--------------------------------------+
```

---

## 2. Chuẩn bị host

### 2.1. MQTT broker

Broker mosquitto phải chạy trên host (cho mode auto) — xem guide [MQTT broker trên host](04-mqtt-broker-tren-host.md). Cần cả
`mosquitto-clients` để có `mosquitto_pub`:

```bash
sudo apt install mosquitto mosquitto-clients
```

### 2.2. Build file `.swu` mới

Nâng version rồi build:

```bash
echo 'OTA_SW_VERSION = "0.2.0"' >> conf/local.conf
bitbake update-image
# -> tmp/deploy/images/bbb-home-gateway/update-image-bbb-home-gateway-0.2.0.swu
```

### 2.3. Chạy server firmware

Tool test — cấu hình fix cứng trong file, không có tham số dòng lệnh:

```bash
cd tools/ota-server
python3 ota_server.py          # tự tạo thư mục release/ ở gốc repo

# Copy file .swu vào release/ (ngang hàng tools/; KHÔNG cần restart, server tự nhận):
cp /path/to/tmp/deploy/images/bbb-home-gateway/update-image-bbb-home-gateway-0.2.0.swu ../../release/
```

Kiểm tra:

```bash
curl -s http://192.168.137.1:8000/manifest.json        # thấy version + url + sha256
mosquitto_sub -h 127.0.0.1 -t ota/latest -C 1           # thấy manifest retained
```

---

## 3. Cấu hình trên BBB

App đọc các biến môi trường (đã set trong `home-dashboard.service`):

| Env | Mặc định | Ý nghĩa |
|---|---|---|
| `OTA_MANIFEST_URL` | `http://192.168.137.1:8000/manifest.json` | endpoint manual check |
| `OTA_MQTT_TOPIC` | `ota/latest` | topic auto |
| `MQTT_BROKER_HOST` / `MQTT_BROKER_PORT` | `192.168.137.1` / `1883` | broker (dùng chung với sensor) |

Đổi giá trị: sửa [home-dashboard.service](../../meta-bsp/recipes-qtapp/home-dashboard/files/home-dashboard.service) rồi build lại image, hoặc có thể edit tạm thời trên board: `systemctl edit`.

Version hiện tại app lấy từ `/etc/sw-versions`:

```bash
cat /etc/sw-versions      # vd: homegateway 0.1.0
```

---

## 4. Mode Manual

1. Trên màn **Settings**, để toggle OTA ở chế độ **Manual** (mặc định).
2. Bấm nút **Check for updates**:
   - App sẽ get `/manifest.json`, compare version với `/etc/sw-versions`.
   - Có bản mới -> popup *"New version available"* (vd `0.1.0 -> 0.2.0`).
   - Không có -> nút hiện *"Up to date"* khoảng 2.5s rồi trở lại.
   - Lỗi mạng -> *"Check failed"* (xem log app để biết lý do).
3. Bấm **Update now** trong popup -> chuyển sang màn tiến độ (mục 6).

---

## 5. Mode Auto

> Chế độ Auto/Manual được lưu ở file cấu hình **`/data/ota/config.json`** — `/data` giữ nguyên qua OTA + reboot, nên chọn auto một lần là giữ mãi. Kiểm tra: `cat /data/ota/config.json`.

1. Trên **Settings**, gạt toggle sang **Automatic**.
2. Khi server publish version mới hơn lên `ota/latest`, app tự bật popup ngay (không cần bấm gì), kể cả khi đang ở màn dashboard. Vì message là *retained*, board vừa kết nối / vừa bật auto cũng nhận được bản mới nhất ngay.
3. Bấm **Update now** -> hiện thị màn hình progress update firmware.

> Release bản mới: chỉ cần copy `.swu` version cao hơn vào thư mục `release/`; trong vài giây server republish -> board sẽ tự động hiện popup.

---

## 6. Màn hình progress update firmware

3 bước: **Downloading -> Verifying -> Flashing**, rồi **Complete**.
- **Downloading** — tải `.swu` từ host (%, thật). Có thể **Cancel**.
- **Verifying** — bắt đầu đẩy `.swu` lên SWUpdate daemon `localhost:8080` (daemon parse `sw-description`, check hw-compatible).
- **Flashing** — daemon ghi vào slot inactive; % thật theo `uploadProgress` (daemon nhận stream theo tốc độ ghi).Không cho Cancel (tránh hỏng slot đang ghi).
- **Complete** — hiện *"<version> installed"* + nút **Reboot now**.

Theo dõi phía board trong lúc cài:

```bash
journalctl -u swupdate -f
fw_printenv active_slot ustate boot_count    # sau flash: ustate=1, active_slot đã đổi
```

Bấm **Reboot now** -> app gọi `reboot` -> U-Boot boot slot mới (xem mục 5/6 tại guide [OTA Update workflow](02-ota-update-workflow.md) cho phần U-Boot & confirm.

---

## 7. Verify sau update

```bash
cat /etc/sw-versions                         # version mới
fw_printenv active_slot ustate boot_count    # ustate=0 (đã commit), boot_count=0
mount | grep ' / '                           # rootfs slot mới
```

Trên màn Settings, version firmware hiển thị cũng là version mới.

---

## 8. Debug

| Triệu chứng | Kiểm tra |
|---|---|
| Manual check báo *Check failed* | `curl http://<host>:8000/manifest.json` từ board được không? đúng `OTA_MANIFEST_URL`? |
| Auto không popup | `mosquitto_sub -h <host> -t ota/latest -C 1` có manifest? toggle đang Automatic? version có cao hơn không? |
| Tải xong báo *Sai sha256* | File trên server hỏng hoặc manifest cũ — server tự tính lại sha256 mỗi lần file đổi, thử restart server |
| Flashing đứng ở 0% | `journalctl -u swupdate -f` xem daemon có nhận file không; daemon kẹt thì `uploadProgress` không tăng. Hoàn tất vẫn bắt qua `fw_printenv -n ustate` |
| *Update failed* | Lý do hiển thị trên màn; chi tiết ở `journalctl -u swupdate` (vd hardware mismatch) |
| Flash xong không vào slot mới | `fw_printenv active_slot ustate` — `switch-slot.sh` có chạy? |

---

## 8b. Test nhanh không cần nâng version

Bình thường chi thực hiện update khi version trong manifest cao hơn `/etc/sw-versions`, nên muốn test lại phải nâng version mỗi lần -> rất bất tiện.

Bật env `OTA_FORCE_UPDATE=1` để app bỏ qua việc so sánh version ->  Khi đó cứ copy file `.swu` mới vào server và bấm
*Check for updates* (hoặc đợi popup auto) là cài được, không cần đổi version.

Bật/tắt ngay trên board đang chạy (không cần build lại image):

```bash
# Bật chế độ test
systemctl edit home-dashboard        # thêm:  Environment=OTA_FORCE_UPDATE=1
systemctl restart home-dashboard

# Tắt khi đã test ổn định -> về hành vi bình thường
systemctl revert home-dashboard
systemctl restart home-dashboard
```