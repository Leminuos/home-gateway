# Smart Home Gateway

## Tổng quan

**Smart Home Gateway** là một hệ thống IoT gateway chạy trên BeagleBone Black. Thiết bị có màn hình TFT cảm ứng hiển thị dashboard (nhiệt độ, độ ẩm, ánh sáng), đọc cảm biến qua I2C, gửi/nhận dữ liệu qua MQTT và hỗ trợ cập nhật firmware từ xa (OTA) theo cơ chế A/B có rollback tự động. Toàn bộ hệ điều hành được build bằng Yocto kirkstone.

Repo gồm 3 thành phần chính:

| Thư mục | Vai trò |
|---|---|
| `meta-bsp/` | Yocto layer board support: định nghĩa machine, distro, kernel, network và recipe đóng gói Qt app |
| `meta-ota/` | Yocto layer OTA: logic A/B, partition layout |
| `home-gateway-app/` | Source code ứng dụng Qt5 |

---

## Giao diện

Ứng dụng chạy fullscreen trên màn hình TFT 240×320, gồm 3 màn hình chính điều hướng qua thanh nav dưới cùng (Home / Dashboard / Settings) và luồng cập nhật OTA. Các ảnh dưới đây được chụp trực tiếp từ framebuffer của board.

### Màn hình Home

Dashboard nhiệt độ / độ ẩm / ánh sáng đọc từ cảm biến, kèm trạng thái online.

<img src="assets/img/home-screen.png" width="200" height="267">

### Màn hình Settings

Chỉnh độ sáng, chế độ OTA, version hiện tại, thông tin thiết bị (MAC) và nút *Check for updates*.

| Cài đặt | Cài đặt |
|:---:|:---:|
| <img src="assets/img/setting-screen-1.png" width="200" height="267"> | <img src="assets/img/setting-screen-2.png" width="200" height="267"> |

### Dashboard biểu đồ

Biểu đồ lịch sử số liệu cảm biến, chọn khoảng thời gian 1h / 6h / 24h.

| Temperature | Humidity | Light |
|:---:|:---:|:---:|
| <img src="assets/img/dashboard-screen-1.png" width="200" height="267"> | <img src="assets/img/dashboard-screen-2.png" width="200" height="267"> | <img src="assets/img/dashboard-screen-3.png" width="200" height="267"> |

### Luồng cập nhật OTA

| Phát hiện bản mới | Đang cài | Hoàn tất |
|:---:|:---:|:---:|
| <img src="assets/img/ota-popup.png" width="200" height="267"> | <img src="assets/img/ota-progress.png" width="200" height="267"> | <img src="assets/img/ota-success.png" width="200" height="267"> |
| Popup hiện khi có version mới | Tiến độ download → flash | Cài xong, hiện nút *Reboot now* |

---

## Yêu cầu hệ thống

### Phần cứng (thiết bị target)

- BeagleBone Black
- Màn hình giao tiếp SPI TFT ILI9341 240×320 có resistive touch
- Cảm biến nhiệt độ/độ ẩm SHT30
- Cảm biến ánh sáng BH1750
- Backlight điều khiển qua PWM
- Thẻ SD để boot
- Cáp serial UART để xem console — baudrate `115200 8N1`

### Phần mềm (máy build host)

- Ubuntu 20.04 / 22.04 (khuyến nghị), tối thiểu **8GB RAM** (khuyến nghị 16GB) và **~50GB** dung lượng trống cho thư mục build
- Bộ dependency của Yocto kirkstone:

  ```bash
  sudo apt update
  sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
      chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
      iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 \
      xterm python3-subunit mesa-common-dev zstd liblz4-tool file locales
  sudo locale-gen en_US.UTF-8
  ```

- `bmaptool` hoặc `dd` để flash SD card

---

## Cài đặt và build

### 1. Clone các layer phụ thuộc

Repo này không chứa Poky hoặc các meta-layer phụ thuộc, cần clone riêng:

```bash
mkdir ~/yocto && cd ~/yocto

git clone -b kirkstone https://git.yoctoproject.org/poky
git clone -b kirkstone https://git.openembedded.org/meta-openembedded
git clone -b kirkstone https://github.com/meta-qt5/meta-qt5.git
git clone -b kirkstone https://github.com/sbabic/meta-swupdate.git

git clone <repo-url> home-gateway   # repo dự án này
```

### 2. Khởi tạo build directory

```bash
cd ~/yocto
source poky/oe-init-build-env build
```

Mọi lệnh `bitbake` sau đây chạy từ thư mục `build/` này.

### 3. Khai báo các layer

Thêm các layer của dự án vào `conf/bblayers.conf` theo đúng thứ tự:

```bitbake
BBLAYERS ?= " \
  /home/<user>/yocto/poky/meta \
  /home/<user>/yocto/poky/meta-poky \
  /home/<user>/yocto/poky/meta-yocto-bsp \
  /home/<user>/yocto/meta-openembedded/meta-oe \
  /home/<user>/yocto/meta-openembedded/meta-networking \
  /home/<user>/yocto/meta-openembedded/meta-multimedia \
  /home/<user>/yocto/meta-openembedded/meta-python \
  /home/<user>/yocto/meta-qt5 \
  /home/<user>/yocto/meta-swupdate \
  /home/<user>/yocto/home-gateway/meta-bsp \
  /home/<user>/yocto/home-gateway/meta-ota \
"
```

### 4. Cấu hình build

Mở `conf/local.conf` và set machine, distro của dự án:

```bitbake
MACHINE = "bbb-home-gateway"
DISTRO  = "home-gateway"

# Optional — bật dev tools (i2c-tools, evtest, tslib-tests, systemd-analyze)
DEVELOPMENT_BUILD = "1"

# Optional — override version OTA
OTA_SW_VERSION = "0.1.0"
```

### 5. Build image

```bash
bitbake core-image-home-gateway
```

Lần đầu mất khoảng 2–4 tiếng. Các lần sau dùng lại sstate-cache nên nhanh hơn nhiều. Sau khi xong, các artifact nằm ở `tmp/deploy/images/bbb-home-gateway/`, quan trọng nhất là file flash SD card:

```
core-image-home-gateway-bbb-home-gateway.wic        ◄── file flash vào SD
core-image-home-gateway-bbb-home-gateway.wic.bmap   ◄── đi kèm để bmaptool flash nhanh
```

Để đóng gói file OTA `.swu`: `bitbake update-image`.

### 6. Flash SD card

**Cảnh báo:** xác định đúng device của SD card bằng `lsblk` trước khi flash — flash nhầm sang ổ cứng của host là mất toàn bộ dữ liệu. Unmount mọi partition đã auto-mount trước (`sudo umount /dev/sdX*`).

```bash
cd ~/yocto/build/tmp/deploy/images/bbb-home-gateway

# Cách 1 — bmaptool (nhanh, khuyến nghị)
sudo bmaptool copy core-image-home-gateway-bbb-home-gateway.wic /dev/sdX

# Cách 2 — dd
sudo dd if=core-image-home-gateway-bbb-home-gateway.wic of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

(`/dev/sdX` là device SD card đã xác định ở trên.)

### 7. Boot lần đầu

1. Cắm SD vào BeagleBone Black.
2. **Giữ nút S2** (gần khe SD) khi cấp nguồn để BBB ưu tiên boot từ SD thay vì eMMC.
3. Cắm UART để xem console.
4. Cấp nguồn 5V. Khi console hiện `bbb-home-gateway login:` là boot xong — đăng nhập user `root` (không có password).

---

## Build SDK để cross-compile app trên host

Để dev nhanh ứng dụng `home-dashboard` mà không phải build lại toàn bộ image, dùng SDK do Yocto sinh ra. SDK cung cấp cross-compiler, các thư viện target và script `environment-setup-*` để cross-build app bằng CMake ngay trên host.

### 1. Build bộ cài SDK

```bash
bitbake meta-toolchain-qt5
```

Lệnh này sinh ra một bộ cài self-extracting `.sh` tại `tmp/deploy/sdk/`:

```
home-gateway-glibc-x86_64-meta-toolchain-qt5-cortexa8hf-neon-bbb-home-gateway-toolchain-<version>.sh
```

### 2. Cài SDK

Chạy bộ cài và chọn thư mục cài (mặc định `/opt/poky/<version>`):

```bash
./tmp/deploy/sdk/home-gateway-glibc-x86_64-meta-toolchain-qt5-*-toolchain-*.sh
```

### 3. Source environment trước khi build

Mỗi khi mở terminal mới để build app, source script environment của SDK:

```bash
source /opt/poky/<version>/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
```

Sau khi source, các biến `CC`, `CXX`, `OECORE_TARGET_SYSROOT`...trỏ tới cross-toolchain. Lúc này có thể cross-build app trực tiếp bằng CMake hoặc tiện hơn là dùng script `tools/deploy-home-dashboard.sh` để build và deploy luôn lên board.

---

## Các tool trong dự án

Các tool hỗ trợ phát triển nằm trong thư mục `tools/`.

### Build & deploy nhanh Qt app

Script: `tools/deploy-home-dashboard.sh`. Cross-build ứng dụng `home-gateway-app` bằng toolchain Yocto rồi inject binary thẳng lên board qua SSH. Dùng để deploy nhanh khi phát triển app mà không phải build và flash lại toàn bộ image. Cần source SDK environment trước:

```bash
source /path/to/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
tools/deploy-home-dashboard.sh [BOARD_IP]
```

Các biến môi trường tuỳ chọn:

| Biến | Ý nghĩa | Mặc định |
|---|---|---|
| `BOARD_IP` | IP board | `192.168.137.100` |
| `BOARD_USER` | user SSH trên board | `root` |
| `BUILD_ONLY=1` | chỉ build, không inject lên board | - |
| `CLEAN=1` | xoá thư mục build trước khi cấu hình lại | - |

### Chụp màn hình TFT

Script: `tools/screenshot-board.sh`. SSH vào board, đọc raw framebuffer (`/dev/fb0`), kéo về host và giải mã thành PNG, lưu tại `assets/tmp/yyyymmdd_hhmmss.png`.

```bash
tools/screenshot-board.sh [BOARD_IP]
```

### OTA firmware server trên host

Script: `tools/ota-server/ota_server.py`. Server Python nhỏ chạy trên host để phục vụ firmware `.swu` cho BBB theo mô hình **pull** (board chủ động tải về). Server làm 3 việc:

- `GET /manifest.json` — trả về thông tin version mới nhất
- `GET /release/<file>.swu` — cho board tải file firmware.
- Publish retained message lên topic MQTT `ota/latest` để app trên board tự bật popup.

Server tự quét thư mục `release/`, chọn file `.swu` có version cao nhất, tính `sha256` + `size`, ghi vào `manifest.json` và republish MQTT mỗi khi phát hiện file mới.

```bash
cd tools/ota-server
python3 ota_server.py

# Release bản mới: chỉ cần copy file .swu vào release/, server tự nhận trong vài giây
cp ~/yocto/build/tmp/deploy/images/bbb-home-gateway/update-image-*.swu ../../release/
```

Cấu hình IP host, port, đường dẫn release, broker MQTT,... sửa trực tiếp ở đầu file `ota_server.py`.
