# 0009 — Chuyển toàn bộ cấu hình runtime từ env var sang JSON config file trên /data

## Tóm tắt

Thay thế các env var trong `home-dashboard.service` bằng hai file JSON nằm trên partition `/data`, tồn tại qua OTA và reboot. Thêm class `MqttSettings` cho cấu hình kết nối MQTT, mở rộng `OtaSettings` với các field OTA mới. Thêm auto-polling HTTP cho chế độ tự động.

---

## Lý do

Trước đây tất cả thông số runtime (broker host/port, manifest URL, MQTT topic...) được set cứng trong `home-dashboard.service` dưới dạng env var. Hệ quả: thay đổi bất kỳ thông số nào đều phải rebuild image và reflash toàn bộ thiết bị. Với thiết bị đã deploy, điều này là không thực tế.

Giải pháp: đặt các thông số này vào file JSON trên partition `/data`. Partition `/data` không bị mất dữ liệu khi OTA update, nên config tồn tại qua mọi lần update. Để thay đổi thông số, chỉ cần SSH vào thiết bị, sửa file JSON, không cần rebuild.

---

## Cấu trúc hai file config

**`/data/config/mqtt.json`** — thông số kết nối MQTT broker:

```json
{
  "host": "192.168.137.10",
  "port": 1883
}
```

Tách riêng vì đây là thông số hạ tầng mạng — không liên quan trực tiếp đến luồng OTA, và có thể cần thay đổi độc lập.

**`/data/config/ota.json`** — tuỳ chọn luồng OTA:

```json
{
  "auto-mode": false,
  "manifest-url": "http://192.168.137.10:8000/manifest.json",
  "mqtt-topic": "ota/latest",
  "force-update": false,
  "polling-interval-sec": 60
}
```

| Field | Mô tả | Default |
|---|---|---|
| `auto-mode` | Tự động popup khi có bản mới | `false` |
| `manifest-url` | URL HTTP lấy manifest firmware | `http://192.168.137.10:8000/manifest.json` |
| `mqtt-topic` | Topic broker publish khi có firmware mới | `ota/latest` |
| `force-update` | Bỏ qua so sánh version, dùng khi test | `false` |
| `polling-interval-sec` | Chu kỳ poll manifest tự động (giây); `0` = tắt | `60` |

> Các key trong file config dùng kebab-case (`auto-mode`, `mqtt-topic`…). Tên method C++ tương ứng vẫn là camelCase (`autoMode()`, `mqttTopic()`) — chỉ chuỗi key JSON là kebab-case.

---

## Chi tiết thay đổi C++

### `OtaConfig.h`

Xóa các hàm đọc env var `OTA_MANIFEST_URL`, `OTA_MQTT_TOPIC`, `OTA_FORCE_UPDATE` vì các giá trị này nay đọc từ JSON. Thêm `mqttConfigFile()` trả về đường dẫn `/data/config/mqtt.json`. Đường dẫn `configFile()` đổi thành `/data/config/ota.json`.

### `MqttSettings.h` / `MqttSettings.cpp` (mới)

Class quản lý `/data/config/mqtt.json`. Pattern giống `OtaSettings`: `load()` đọc JSON (thiếu file → giữ default), `save()` ghi với mkpath. API: `host()`, `port()`, `setHost()`, `setPort()`.

### `OtaSettings.h` / `OtaSettings.cpp`

Bỏ field `mqttHost`/`mqttPort` (chuyển sang `MqttSettings`). Thêm:

- `manifestUrl()` / `setManifestUrl()`
- `mqttTopic()` / `setMqttTopic()`
- `forceUpdate()` / `setForceUpdate()`
- `pollingIntervalSec()` / `setPollingIntervalSec()`

### `OtaManager.h` / `OtaManager.cpp`

Thêm `mAutoPoller` (`QTimer`): khi `autoMode = true` và `pollingIntervalSec > 0`, timer gọi HTTP GET manifest định kỳ — bổ sung cho MQTT retained (phát hiện bản mới ngay cả khi MQTT gián đoạn).

Thêm `mManifestFromUser` (`bool`): phân biệt request do user trigger (emit `upToDate` / `checkFailed`) và auto poll (im lặng). Signature `checkForUpdate(bool fromUser = true)`.

Thay `OtaConfig::mqttTopic()` → `mSettings.mqttTopic()`, `OtaConfig::forceUpdate()` → `mSettings.forceUpdate()`, `OtaConfig::manifestUrl()` → `mSettings.manifestUrl()`.

### `MainDashboard.h` / `MainDashboard.cpp`

`init()` đổi thành `init(const QString &mqttHost, int mqttPort)` — nhận tham số thay vì tự đọc env var.

### `main.cpp`

```cpp
MqttSettings mqttCfg;
mqttCfg.load();
dashboard->init(mqttCfg.host(), mqttCfg.port());
ota->start(mqttCfg.host(), mqttCfg.port());
```

Load `MqttSettings` một lần, truyền vào cả sensor dashboard và OtaManager.

### `home-dashboard.service`

```diff
-Environment=MQTT_BROKER_HOST=192.168.137.1
-Environment=MQTT_BROKER_PORT=1883
-Environment=OTA_MANIFEST_URL=http://192.168.137.1:8000/manifest.json
-Environment=OTA_MQTT_TOPIC=ota/latest
-Environment=OTA_FORCE_UPDATE=1
 After=multi-user.target data.mount
```

Service file giờ chỉ còn tslib env vars. Tất cả OTA/MQTT config đọc từ `/data/config/`.

### Sửa IP (`192.168.137.1` → `192.168.137.10`)

Topology thực tế: broker và OTA server chạy trên Ubuntu VM (`192.168.137.10`), không phải Windows host (`192.168.137.1`). Sửa trong:

- `tools/ota-server/ota_server.py` — `HOST_IP`
- `tools/ota-server/manifest.json` — `url`
- Default value trong `MqttSettings::host()` và `OtaSettings::manifestUrl()`

---

## Thay đổi thông số kết nối sau khi deploy

SSH vào thiết bị, chỉnh file JSON, restart service:

```bash
vi /data/config/mqtt.json        # đổi host/port broker
vi /data/config/ota.json         # đổi manifestUrl, bật/tắt polling, v.v.
systemctl restart home-dashboard
```

Không cần rebuild image.
