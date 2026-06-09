# 0011 — Thêm setting.json lưu cấu hình home-dashboard (độ sáng)

## Tóm tắt

Thêm file config thứ ba `/data/config/setting.json` để lưu các tuỳ chọn của giao diện home-dashboard — bắt đầu với độ sáng màn hình. Thêm class `AppSettings` quản lý file này, wire vào `SettingsWidget` để khôi phục độ sáng lúc khởi động và lưu lại khi user chỉnh. File mặc định được build sẵn vào partition `/data`.

---

## Lý do

Trước đây độ sáng màn hình chỉ lấy từ giá trị mặc định trong `SettingsDashboard.ui` (72%) mỗi lần khởi động — user chỉnh xong, reboot là mất. Cần lưu các tuỳ chọn UI để tồn tại qua reboot và OTA.

`mqtt.json` và `ota.json` đã có nhưng thuộc về kết nối/luồng OTA. Cấu hình giao diện là nhóm khác (hành vi hiển thị, không liên quan mạng/OTA), nên tách thành file riêng `setting.json` để rõ trách nhiệm và dễ mở rộng (sau này thêm timeout tắt màn hình, theme…).

---

## Cấu trúc file

**`/data/config/setting.json`**:

```json
{
  "brightness": 72
}
```

| Field | Mô tả | Default |
|---|---|---|
| `brightness` | Độ sáng màn hình theo thang slider `0..100` | `72` |

---

## Chi tiết thay đổi

### `AppSettings.h` / `AppSettings.cpp` (mới — `src/settings/`)

Class quản lý `/data/config/setting.json`, cùng pattern với [MqttSettings / OtaSettings](0009-runtime-config-to-json.md): `load()` đọc JSON (thiếu/hỏng → giữ default), `save()` ghi với `mkpath`. API: `brightness()` (default `72`), `setBrightness()`. Đường dẫn file có thể override qua env `APP_CONFIG_FILE`.

### `SettingsDashboard.h` / `SettingsDashboard.cpp`

- Thêm member `AppSettings mAppSettings`.
- Tách logic độ sáng làm ba hàm:
  - `applyBrightness(int)` — cập nhật nhãn `%` + duty cycle PWM, **không** ghi file.
  - `onBrightnessChanged(int)` — gọi khi `valueChanged` (kéo slider): apply live, chưa ghi file để tránh I/O liên tục.
  - `onBrightnessReleased()` — gọi khi `sliderReleased` (thả tay): ghi `setting.json` một lần.
- Constructor: sau khi init PWM, `load()` settings rồi set slider về `brightness()` đã lưu (block signal để không kích hoạt save khi khôi phục), gọi `applyBrightness()` một lần để áp ngay duty cycle.

Tách `valueChanged` (live) và `sliderReleased` (save) để mỗi lần chỉnh chỉ ghi `/data` đúng một lần thay vì ghi liên tục theo từng nấc slider.

### `data-partition.bb` + `files/setting.json`

Thêm `setting.json` vào `SRC_URI` và copy vào `data-root/config/` trong `do_compile` để `mke2fs -d` bake sẵn vào ext4 image. Xem cơ chế build /data tại [0010-data-partition-build-time.md](0010-data-partition-build-time.md).

---

## Phân tích ảnh hưởng

- Độ sáng giờ tồn tại qua reboot và OTA (vì nằm trên `/data`, không bị OTA đụng).
- Thiết bị đã có `/data` cũ (build trước thay đổi này) sẽ **không** có `setting.json` — `AppSettings::load()` xử lý gọn: thiếu file → dùng default 72, và lần đầu user chỉnh slider sẽ tự tạo file qua `save()`.
- Pattern `AppSettings` đồng nhất với `MqttSettings`/`OtaSettings`, dễ thêm field mới cho UI sau này.
