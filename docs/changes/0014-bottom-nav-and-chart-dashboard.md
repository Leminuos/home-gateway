# 0014 — Bottom navigation bar + màn Dashboard chart

## Tóm tắt

Thay nút *Settings* đơn lẻ ở cuối màn Home bằng **bottom navigation bar** cố định gồm 3 tab **Home / Dashboard / Settings**. Thêm một màn **Dashboard mới** vẽ **biểu đồ** nhiệt độ / độ ẩm / ánh sáng bằng **Qt Charts**, có bộ chọn khoảng thời gian **1h / 6h / 24h**. Màn Home giữ nguyên 3 card số liệu hiện tại; màn Settings giữ nguyên (chỉ chuyển nút *Check for updates* vào trong vùng scroll để nhường chỗ dưới cùng cho nav bar). Lịch sử số liệu cho chart lấy từ `SensorLogger` (CSV trên `/data`) đã được mở rộng để đọc lại được.

## Lý do

- Điều hướng cũ chỉ có Home ⇄ Settings qua một nút, không có chỗ cho màn xem xu hướng số liệu. Bottom nav bar cho phép mở rộng số screen chính và chuyển nhanh giữa chúng.
- Người dùng cần xem diễn biến nhiệt độ / độ ẩm / ánh sáng theo thời gian, không chỉ giá trị tức thời — nên thêm màn chart với khoảng thời gian chọn được.
- Đã có sẵn `SensorLogger` ghi CSV số liệu lên `/data`; tái dùng làm nguồn lịch sử cho chart thay vì thêm cơ chế lưu trữ thứ hai (tránh ghi `/data` trùng lặp).

## Thay đổi quan trọng

- **Qt Charts dependency**: thêm `Charts` vào `find_package`/`target_link_libraries` (`CMakeLists.txt`), `qtcharts` vào `DEPENDS` của recipe `home-dashboard_1.0.bb` và vào `IMAGE_INSTALL` của `core-image-home-gateway.bb` (runtime `libQt5Charts`). `qtcharts` đã có sẵn trong Yocto build và SDK (Qt 5.15), không cần thêm layer.
- **BottomNavBar** (shared widget mới, `src/ui/BottomNavBar.*`): `QHBoxLayout` 3 `QToolButton` checkable dạng dọc (`Qt::ToolButtonTextUnderIcon` — icon trên, text dưới) dùng `QButtonGroup` exclusive. Phát `navRequested(int)`; slot `setActive(int)` để đồng bộ tab khi điều hướng đến từ luồng khác. Icon dùng `home.png`, `dashboard.png`, `settings.png`.
- **ChartDashboard** (screen mới, `src/ui/ChartDashboard.*`): mỗi metric một `QChartView` riêng (trục Y auto-range riêng vì khác thang đo), xếp dọc trong `QScrollArea`. Trục X là `QDateTimeAxis` theo cửa sổ `[now - range, now]`. Hàng nút **1h / 6h / 24h** đổi cửa sổ thời gian. Giữ buffer thô (từ `SensorLogger::samples()` + cập nhật realtime qua `appendSample`) rồi **dựng lại series với bước gom mẫu khác nhau theo khoảng** (1h → 2 phút/điểm, 6h → 10 phút/điểm, 24h → 30 phút/điểm; gom bucket + lấy trung bình) để chart không quá dày, dễ nhìn. Bật `QScroller` (kéo ngón tay để cuộn như Settings) và cho `QChartView` trong suốt với chuột để gesture truyền tới viewport; font/tick rút gọn cho màn 240px.
- **SensorLogger mở rộng** (`src/logging/SensorLogger.*`): chuyển thành `QObject`; vẫn ghi CSV mỗi lần `append`, nhưng thêm buffer RAM lấy mẫu thưa **1 mẫu/phút, cap 24h** làm nguồn cho chart. Thêm `load()` (đọc lại CSV, downsample ≥60s, bỏ mẫu ngoài 24h), `samples()`, và signal `sampleAdded(t, temp, humi, lux)`.
- **MainDashboard (Home)**: bỏ `QPushButton settingsButton` khỏi `.ui` và bỏ signal `settingsRequested`; thêm accessor `sensorLogger()` để `main.cpp` nạp lịch sử + nối signal cho chart. Logic đọc sensor / publish MQTT / ghi CSV giữ nguyên.
- **SettingsDashboard**: chuyển `checkUpdatesButton` từ ngoài vào trong `scrollLayout` (cuộn cùng nội dung) để không bị nav bar che; logic không đổi.
- **main.cpp**: thêm `BottomNavBar` dưới cùng `rootLayout`; stack xếp theo thứ tự Home=0 / Dashboard=1 / Settings=2 / Progress=3 (khớp index nav). Nối `navRequested → setCurrentIndex`, `SensorLogger::sampleAdded → ChartDashboard::appendSample`, `SettingsWidget::backRequested → Home`. Ẩn nav bar khi vào màn OTA Progress, hiện lại khi cancel.
- **Resources/QSS**: khai báo `home.png`, `dashboard.png` và 2 file QSS mới (`bottomnav.qss`, `chart.qss`) trong `resources.qrc`; load thêm 2 QSS trong `main.cpp`. Bỏ rule `QPushButton#settingsButton` cũ trong `dashboard.qss` (nút đã xóa, tránh trùng tên với nút trong nav bar).

## Phân tích ảnh hưởng

- **Image size**: thêm `libQt5Charts` (+ phụ thuộc) làm tăng dung lượng rootfs. Cần build lại image và SDK đã bao gồm `qtcharts`.
- **Ghi flash `/data`**: không tăng so với trước — vẫn chỉ `SensorLogger` ghi CSV (nhịp như cũ). Buffer chart nằm trong RAM (1 mẫu/phút), không ghi thêm file.
- **Bộ nhớ**: chart giữ tối đa ~1440 điểm/metric (24h × 1/phút). Khi mở app, `load()` parse CSV và downsample nên không nạp toàn bộ file vào series.
- **Điều hướng khi OTA**: nav bar bị ẩn trong lúc Progress để tránh rời màn cập nhật; trở lại bình thường khi cancel. Khi update xong (`rebootRequested`) thiết bị reboot nên không cần khôi phục nav.
- **Render trên linuxfb**: Qt Charts vẽ qua QGraphicsView/QPainter (raster), không cần OpenGL — chạy được trên framebuffer ILI9341.
