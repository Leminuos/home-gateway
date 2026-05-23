# Qt5 dashboard cho Smart Home Gateway

## Overview

Ứng dụng Qt5 chạy fullscreen trên màn hình TFT 240×320 của BBB. Vai trò:
- Hiển thị dashboard nhiệt độ / độ ẩm / ánh sáng đọc từ cảm biến I2C.
- Publish số liệu cảm biến lên MQTT broker; hiển thị trạng thái online/offline.
- Chỉnh độ sáng màn hình (PWM) và cập nhật firmware OTA ngay trên UI.

Render qua linuxfb + tslib (không X11/OpenGL), build bằng CMake với cross-toolchain Yocto. Binary tên `home-gateway-app`, được recipe [`home-dashboard`](../meta-bsp/recipes-qtapp/home-dashboard/) (trong `meta-bsp`) đóng gói và chạy bằng systemd.

---

## Cấu trúc

```
home-gateway-app/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                # entrypoint: dựng UI, khởi tạo OtaManager, nối signal
│   ├── ui/                     # các Screen + shared widget (.h/.cpp/.ui)
│   │   ├── MainDashboard.*     # class Widget — màn chính: đọc sensor, publish MQTT
│   │   ├── SettingsDashboard.* # SettingsWidget — backlight + chế độ OTA + version
│   │   ├── StatusHeader.*      # shared widget — chỉ báo online/offline
│   │   ├── FirmwareUpdateProgress.*  # màn tiến độ update (download -> verify -> flash -> done)
│   │   └── FirmwareUpdatePopup.*     # popup version mới
│   ├── drivers/
│   │   ├── TemperatureHumiditySensor.*  # SHT30 qua I2C (0x44)
│   │   ├── LightSensor.*                # BH1750 qua I2C (0x23)
│   │   └── BacklightPwm.*               # backlight qua sysfs PWM
│   ├── network/
│   │   ├── MqttClient.*        # wrap libmosquitto, loop bằng QTimer
│   │   └── NetworkInfo.*       # đọc MAC address
│   └── ota/
│       ├── OtaManager.*        # điều phối: phát hiện -> tải .swu -> POST daemon -> reboot
│       ├── OtaConfig.h         # cấu hình qua env (URL manifest, topic, socket…)
│       ├── OtaSettings.*       # lưu auto/manual ở /data/ota/config.json
│       ├── FirmwareManifest.*  # parse manifest JSON + so sánh version
│       └── SwupdateProgressClient.*  # đọc tiến độ từ socket /tmp/swupdateprog
└── resources/                  # resources.qrc — icons + QSS stylesheet
```

---

## Thành phần theo thư mục

- **`src/main.cpp`** — Dựng `QWidget` root cố định 240×320 gồm `StatusHeader` + `QStackedWidget` (3 screen: MainDashboard, SettingsDashboard, FirmwareUpdateProgress). `FirmwareUpdatePopup` là overlay phủ toàn màn (con trực tiếp của root). Tạo `OtaManager` và nối toàn bộ signal/slot giữa UI <-> OTA tại đây — main là nơi điều phối, các widget không biết nhau.
- **`src/ui/`** — Tầng giao diện (Qt Widgets + Qt Designer). Mỗi Screen là một `QWidget` có file `.ui` riêng:
  - **MainDashboard**: mỗi 3s đọc sensor, cập nhật label và publish `sensor/temp` · `sensor/humi` · `sensor/lux` (retained); hiện thị trạng thái online/offline theo trạng thái MQTT.
  - **SettingsDashboard**: slider điều chỉnh backlight, toggle OTA Auto/Manual, hiện thị version hiện tại, nút nhấn *Check for updates*.
  - **StatusHeader** / **FirmwareUpdatePopup**: shared widget dùng chung.
  - **FirmwareUpdateProgress**: view thuần, được `OtaManager` điều phối qua các slot `start -> setDownloadProgress -> enterVerify -> enterFlash -> complete/fail`.
- **`src/drivers/`** — Đọc/ghi phần cứng trực tiếp:
  - Đọc cảm biến qua `/dev/i2c-1` (SHT30 `0x44`, BH1750 `0x23`)
  - Backlight set duty cycle qua sysfs PWM.
- **`src/network/`** — `MqttClient` bọc `libmosquitto`: gọi `mosquitto_loop()` định kỳ bằng `QTimer`, tự reconnect.
- **`src/ota/`** — OTA **pull-model**: app chủ động phát hiện bản mới (Auto = MQTT retained `ota/latest`; Manual = HTTP GET manifest), tải `.swu`, **POST `127.0.0.1:8080`** cho SWUpdate daemon flash, theo dõi tiến độ qua socket rồi hiện nút *Reboot now*. Toàn bộ endpoint cấu hình qua env trong [`OtaConfig.h`](src/ota/OtaConfig.h). Chi tiết: [changes/0005](../docs/changes/0005-ota-pull-model-host-server.md).
- **`resources/`** — `resources.qrc` nhúng icon (temperature/humidity/sun/settings) và 4 file QSS (load trong `main.cpp`).

---

## Luồng chính

- **Sensor -> MQTT:** `MainDashboard` timer 3s -> đọc I2C -> cập nhật UI + publish `sensor/*` (retained) nếu đang connected.
- **Điều hướng:** Dashboard ⇄ Settings qua `QStackedWidget`; popup OTA nổi trên mọi screen.
- **OTA:** `OtaManager` phát `updateAvailable` -> popup -> user xác nhận -> chuyển sang `FirmwareUpdateProgress`, `OtaManager` tải + cài và phát tiến độ -> xong hiện *Reboot now* -> `OtaManager::reboot()`.

---

## Build & chạy

- **Dependence:** Qt5 `Widgets` + `Network`, `libmosquitto`. Build bằng CMake (bật AUTOUIC/AUTOMOC/AUTORCC).
- **Cross-build:** không build trên BBB — dùng cross-toolchain Yocto; trên target app do recipe `home-dashboard` (meta-bsp) cài và systemd unit `home-dashboard.service` chạy với `-platform linuxfb -plugin=tslib`.

## Biến môi trường

| Env | Mặc định | Ý nghĩa |
|---|---|---|
| `MQTT_BROKER_HOST` / `MQTT_BROKER_PORT` | `127.0.0.1` / `1883` | Broker (chung cho sensor + OTA auto) |
| `OTA_MANIFEST_URL` | `http://192.168.137.1:8000/manifest.json` | Manifest cho mode manual |
| `OTA_MQTT_TOPIC` | `ota/latest` | Topic retained host publish version mới |
| `OTA_SWUPDATE_URL` | `http://127.0.0.1:8080/upload` | Endpoint POST `.swu` cho daemon |
| `OTA_CONFIG_FILE` | `/data/ota/config.json` | Lưu chế độ Auto/Manual (giữ qua OTA) |
| `OTA_FORCE_UPDATE` | *(off)* | `=1` để bỏ so sánh version khi test |