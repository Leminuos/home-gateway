# OTA firmware server cho host

Server nhỏ chạy trên máy host, phục vụ firmware `.swu` cho BBB theo mô hình pull (BBB chủ động tải về thay vì admin upload lên BBB).

## Server làm gì

- **HTTP** (mặc định `:8000`):
  - `GET /manifest.json` — thông tin version mới nhất (dùng cho mode **manual**).
  - `GET /release/<file>.swu` — tải file firmware.
- **MQTT** (qua `mosquitto_pub`): publish retained message lên topic `ota/latest` với nội dung = manifest, để app trên BBB tự bật popup đối với mode auto.

Server tự quét thư mục `release/`, chọn file `.swu` có version cao nhất, tính `sha256` + `size` và ghi vào file `manifest.json`. Tool re-scan định kỳ, khi version mới nhất đổi sẽ publish lại MQTT.

## Yêu cầu

- Python 3 (chỉ dùng thư viện chuẩn).
- `mosquitto_pub` (gói `mosquitto-clients`) cho mode auto. Thiếu cũng không sao —  server chỉ bỏ qua phần publish MQTT.
- Broker MQTT đang chạy trên host (xem guide [MQTT broker chạy trên host](../../docs/guides/04-mqtt-broker-tren-host.md)).

## Dùng nhanh

```bash
# 1. Chạy server
python3 ota_server.py

# 2. Copy file .swu vào release/
cp /path/to/build/tmp/deploy/images/bbb-home-gateway/update-image-bbb-home-gateway-0.2.0.swu ../../release/
```

Server tự nhận file mới: cập nhật `/manifest.json` và republish MQTT trong vài giây.

## Cấu hình

Sửa trực tiếp khối `CONFIG` ở đầu file [ota_server.py](ota_server.py):

| Hằng số | Mặc định | Ý nghĩa |
|---|---|---|
| `HOST_IP` | `192.168.137.1` | IP của host — nhúng vào `url` manifest |
| `HTTP_PORT` | `8000` | Port HTTP |
| `RELEASE_DIR` | `<repo>/release` | Thư mục chứa `.swu` (gốc repo, tự tạo) |
| `MQTT_BROKER` / `MQTT_PORT` | `127.0.0.1` / `1883` | Broker MQTT |
| `MQTT_TOPIC` | `ota/latest` | Topic retained |
| `WATCH_INTERVAL` | `5` | Số giây giữa các lần re-scan để republish |

## Manifest format

Tại file `tools/ota-server/manifest.json`:

```json
{
  "version": "0.2.0",
  "url":     "http://192.168.137.1:8000/release/update-image-bbb-home-gateway-0.2.0.swu",
  "size":    12582912,
  "sha256":  "9f86d0..."
}
```

App trên BBB so sánh version trên manifest này với `/etc/sw-versions`; nếu cao hơn thì coi là có bản mới. Nếu có `sha256`, app sẽ verify sau khi tải, sai thì huỷ cài.

## Release bản mới

Chỉ cần copy file `.swu` version mới hơn vào thư mục `release/`. Trong vòng `WATCH_INTERVAL` giây server sẽ:
- cập nhật `/manifest.json`,
- publish lại retained `ota/latest` → BBB ở mode auto sẽ tự popup.