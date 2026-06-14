# 0013 — Log số liệu cảm biến ra file CSV trên /data

## Tóm tắt

Thêm class `SensorLogger` ghi mỗi lần đọc cảm biến thành một dòng CSV vào file trên partition `/data`. `MainDashboard` gọi logger trong timer đọc sensor 3s. Vì nằm trên `/data`, log tồn tại qua reboot và OTA.

## Lý do

Trước đây số liệu cảm biến chỉ hiển thị trên UI và publish MQTT — không lưu lịch sử trên thiết bị. Cần lưu lại chuỗi giá trị theo thời gian ngay trên gateway để xem lại offline, không phụ thuộc broker/MQTT. `/data` là partition ghi được và không bị mất khi update OTA.

## Chi tiết thay đổi

### `src/logging/SensorLogger.{h,cpp}` (mới)

Class ghi CSV theo cùng pattern path-resolution như [`AppSettings`](0011-app-settings-json.md): path lấy từ tham số constructor, nếu rỗng thì lấy env `SENSOR_LOG_FILE`, mặc định `/data/logs/sensors.csv`.

```cpp
bool SensorLogger::append(double temperature, int humidity, int lux);
```

- `ensureReady()` (chạy lần đầu): `mkpath` tạo thư mục `/data/logs` nếu chưa có, và ghi header `timestamp,temperature,humidity,lux` nếu file mới/rỗng. Đặt cờ `mReady` để các lần sau bỏ qua bước kiểm tra.
- `append()`: mở file ở chế độ `Append` và ghi một dòng `ISO8601-timestamp,temp(1 chữ số thập phân),humidity,lux`.
- Mọi lỗi I/O chỉ `qWarning()` và trả `false` — không làm gián đoạn vòng đọc sensor/UI.

### `src/ui/MainDashboard.h`

Thêm `#include "logging/SensorLogger.h"` và member `SensorLogger mSensorLogger;`.

### `src/ui/MainDashboard.cpp`

Trong callback timer đọc sensor (3s), sau khi cập nhật label và trước nhánh publish MQTT, gọi:

```cpp
mSensorLogger.append(
    (double)celsiusHumidityValue.temperature,
    (int)celsiusHumidityValue.humidity,
    luxValue
);
```

Ghi log luôn chạy, độc lập với trạng thái kết nối MQTT (khác nhánh publish vốn yêu cầu `client.isConnected()`).

## Phân tích ảnh hưởng

- **CMake**: `CMakeLists.txt` dùng `GLOB_RECURSE` trên `src/*.cpp|*.h` nên file mới trong `src/logging/` tự được biên dịch, không cần sửa build.
- **Thư mục `/data/logs`**: tạo lúc runtime bằng `mkpath`, không cần thêm vào recipe `data-partition`. `/data` là ext4 ghi được nên thao tác hợp lệ.
- **Tần suất & dung lượng**: ghi mỗi 3s ⇒ ~28.800 dòng/ngày, mỗi dòng ~40 byte ⇒ ~1.1 MB/ngày. File tăng không giới hạn — chưa có cơ chế xoay vòng (rotation). Nếu cần chạy dài hạn nên bổ sung logrotate hoặc giới hạn kích thước sau này.
- **Hiệu năng**: mỗi lần ghi mở/đóng file một lần — chấp nhận được với chu kỳ 3s; tránh giữ file handle mở liên tục cho đơn giản và an toàn khi mất điện.
- **OTA/reboot**: log nằm trên `/data` nên không bị xoá khi OTA; chỉ mất khi flash lại toàn bộ disk image.
