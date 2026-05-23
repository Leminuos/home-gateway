# Phân tích và giải thích chi tiết từng thay đổi tối ưu hóa hệ thống

**Mục tiêu:** Giảm rootfs từ 385MB xuống ~120MB

---

## 1. Phân tích yêu cầu hệ thống

### 1.1. Chức năng runtime

Gateway bật lên và chạy đúng những thứ sau:

| Chức năng | Thành phần | Package |
|-----------|------------|---------|
| Hiển thị UI trên ILI9341 (SPI TFT 320x240) | Qt5 framebuffer backend | `qtbase`, `ttf-dejavu-sans`, `fontconfig` |
| Ứng dụng SmartHome | Binary Qt đã compile | `smarthome` |
| MQTT client | Mosquitto daemon + client tools | `mosquitto`, `libmosquitto1`, `mosquitto-clients` |
| Remote access | SSH server | `openssh`, `openssh-sshd` |
| Điều khiển GPIO | Thư viện + CLI tools | `libgpiod`, `libgpiod-tools` |
| Cấu hình mạng | Static IP | `bbb-static-ip` |
| OTA update | SWUpdate (cấu hình riêng) | — |

### 1.2. Hardware interfaces

| Interface | Dùng cho | Kernel module |
|-----------|----------|---------------|
| SPI | ILI9341 display | `kernel-module-ili9341`, `kernel-module-drm-mipi-dbi` |
| GPIO | Relay, LED, nút bấm | Built-in |
| UART | Debug console | Built-in |
| Ethernet | Kết nối mạng | Built-in |
| SD card | Storage | Built-in |

### 1.3. Xác định phần cứng và tính năng không có trên board

BBB Smart Home Gateway **không có**:

- Màn hình HDMI/LVDS -> không cần X11, Wayland, OpenGL, Vulkan
- GPU -> không cần DRM/KMS (trừ MIPI DBI cho ILI9341), OpenGL
- Loa/micro -> không cần audio stack
- WiFi/Bluetooth/NFC/3G -> kết nối qua Ethernet
- PCMCIA/PCI slot -> không có trên BBB
- Bàn phím/chuột vật lý -> quản trị qua SSH
- Desktop environment -> chỉ có Qt app fullscreen trên TFT

Danh sách này là cơ sở cho mọi quyết định loại bỏ ở các mục tiếp theo.

---

## 2. Chuyển đổi base image: core-image-full-cmdline → core-image-base

Đây là thay đổi đầu tiên và có sự thay đổi lớn nhất.

### 2.1. core-image-full-cmdline là gì?

Image recipe của Yocto thiết kế cho hệ thống Linux đầy đủ tính năng dòng lệnh — trải nghiệm giống bản Linux desktop nhưng không có GUI. Nó kế thừa từ `core-image-base` và thêm `packagegroup-core-full-cmdline`, bao gồm:

- **packagegroup-core-full-cmdline-utils**: bash, coreutils đầy đủ (thay busybox), tar, gzip, bzip2, xz, findutils, grep, sed, gawk, procps, psmisc, less, time, diffutils, patch...
- **packagegroup-core-full-cmdline-extended**: mc (Midnight Commander), screen, tmux, lsof, iproute2, iputils, net-tools, ethtool, wget, curl...
- **packagegroup-core-full-cmdline-dev-utils**: strace, ltrace
- **packagegroup-core-full-cmdline-multiuser**: shadow, sudo
- **packagegroup-core-full-cmdline-sys-services**: cronie, logrotate, at, acl...

Dependency chain nghiêm trọng nhất: nhiều package trong các nhóm trên kéo theo `gobject-introspection` → `python3`, và gobject-introspection lại kéo theo `gir-1.0` data (14MB) + `vala` (8.9MB). Đây là lý do Python 17MB tồn tại trên board dù không có app nào gọi Python.

### 2.2. core-image-base là gì?

Image nhẹ hơn, chỉ cung cấp:

- **packagegroup-core-boot**: init system (systemd), udev, base-files,
  busybox (shell + tiện ích cơ bản)
- **packagegroup-base-extended**: kernel, hardware support cơ bản cho board

Không có: Python, mc, bash, full coreutils, gawk, screen, tmux, gobject-introspection, vala, và toàn bộ dependency chain.

### 2.3. Tại sao core-image-base đủ cho Smart Home Gateway?

Đối chiếu với mục 1.1 — không chức năng runtime nào cần Midnight Commander, full coreutils, Python, hay các tiện ích dòng lệnh nâng cao. Busybox cung cấp đủ `sh`, `ls`, `cp`, `cat`, `grep`, `mount`, `reboot`... cho quản trị cơ bản qua SSH.

### 2.4. Những gì được loại bỏ khi chuyển sang core-image-base

| Package/Group | Dung lượng | Mô tả |
|---------------|------------|-------|
| python3 + python3-core | ~17MB | Python runtime — bị kéo bởi gobject-introspection, không app nào dùng |
| gobject-introspection (gir-1.0) | ~14MB | Binding data cho GLib/GObject — dev tool, không cần runtime |
| vala (compiler + data) | ~8.9MB | Ngôn ngữ lập trình — dev tool, không cần trên target |
| mc (Midnight Commander) | ~2.8MB | File manager TUI — convenience tool |
| kbd (keymaps + consolefonts) | ~2.4MB | Keyboard layout đa ngôn ngữ — gateway không có bàn phím |
| coreutils (full) | ~5MB | Busybox thay thế đủ dùng cho gateway |
| bash | ~2MB | Busybox `sh` đủ cho tất cả scripts |
| screen/tmux | ~1MB | Terminal multiplexer — không cần trên gateway |
| Các tiện ích khác (tar, gzip, xz bản full...) | ~5MB | Busybox cung cấp bản nhẹ tương đương |
| **Tổng ước tính** | **~58MB** | |

---

## 3. Loại bỏ distro features không cần thiết

`DISTRO_FEATURES` quyết định những tính năng nào được **build xuyên suốt toàn bộ hệ thống** — từ kernel, thư viện, đến ứng dụng. Khi một feature nằm trong `DISTRO_FEATURES`, mọi package hỗ trợ feature đó sẽ bật nó lên. Loại bỏ ở đây có hiệu ứng lan truyền: một dòng remove ở tầng distro có thể loại bỏ hàng chục packages và thư viện phụ thuộc.

```bitbake
DISTRO_FEATURES:remove = " \
    x11 \
    wayland \
    opengl \
    vulkan \
    bluetooth \
    nfc \
    3g \
    ppp \
    pcmcia \
    wifi \
    alsa \
    pulseaudio \
    gobject-introspection-data \
    pci \
"
```

### 3.1. Nhóm đồ họa / display

| Feature | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `x11` | X Window System — hệ thống hiển thị đồ họa truyền thống trên Linux. Quản lý cửa sổ, input, rendering cho desktop. | ILI9341 dùng framebuffer trực tiếp, không chạy X server. Loại feature này khiến Qt không build XCB plugin, đồng thời loại luôn libX11, libxcb, và toàn bộ `/usr/share/X11` (6.1MB). |
| `wayland` | Display server thế hệ mới thay thế X11. Nhẹ hơn X11 nhưng vẫn là display server đầy đủ. | SPI TFT không cần display server — Qt render thẳng lên framebuffer. |
| `opengl` | API đồ họa 3D, cần GPU hardware hoặc software renderer (mesa). | BBB không có GPU. ILI9341 320x240 không cần 3D rendering. Loại feature này loại libGL, libEGL, mesa. |
| `vulkan` | API đồ họa thế hệ mới thay OpenGL, yêu cầu GPU mạnh hơn. | Không có GPU. |

### 3.2. Nhóm kết nối không dây

| Feature | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `bluetooth` | Bluetooth protocol stack (BlueZ daemon, thư viện, tools). | Board không có Bluetooth hardware. |
| `wifi` | WiFi stack: wpa_supplicant (kết nối WiFi), iw (cấu hình), wireless-regdb (regulatory database). | Gateway kết nối qua Ethernet cáp. |
| `nfc` | Near Field Communication — giao tiếp tầm ngắn (thẻ, mobile payment). | Board không có NFC hardware. |
| `3g` | Cellular modem support: ofono, ModemManager — quản lý kết nối 3G/4G. | Không có modem cellular. |
| `ppp` | Point-to-Point Protocol — giao thức kết nối qua dial-up hoặc cellular modem. | Không có modem, PPP không có mục đích sử dụng. |

### 3.3. Nhóm audio

| Feature | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `alsa` | Advanced Linux Sound Architecture — driver framework và thư viện userspace cho audio (libasound, alsa-utils). | Không có loa, micro, audio codec trên board. |
| `pulseaudio` | Sound server chạy trên ALSA — mix nhiều nguồn âm thanh, quản lý output devices. | Không có audio hardware. PulseAudio phụ thuộc ALSA, loại cả hai. |

### 3.4. Nhóm bus / hardware interface

| Feature | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `pci` | PCI bus enumeration, driver loading, config space access. | BBB dùng bus AM335x nội bộ, không có PCI/PCIe bus. |
| `pcmcia` | Card slot chuẩn cũ (thường trên laptop đời cũ) — PCMCIA/CardBus. | BBB không có PCMCIA slot. |

### 3.5. Nhóm development

| Feature | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `gobject-introspection-data` | File XML mô tả API của thư viện GLib/GObject, dùng để tạo language bindings tự động cho Python, Vala, JavaScript. | App viết bằng C++ với Qt, không cần GObject bindings. Đây là thủ phạm chính kéo Python3 (17MB), gir-1.0 data (14MB), vala (8.9MB) vào rootfs. Loại feature này một mình đã tiết kiệm ~40MB. |

**Tổng impact ước tính:** riêng `x11` + `opengl` + `gobject-introspection-data` tiết kiệm **80–100MB** nhờ loại bỏ toàn bộ dependency chain đi kèm.

---

## 4. Tối ưu kernel configuration

Sau khi loại bỏ feature ở tầng distro, cần kiểm tra xem kernel còn build những driver không cần thiết nào không. Cách kiểm tra: chạy `lsmod` trên board để xem danh sách kernel modules đang được load.

### 4.1. Khảo sát modules đang load

Chạy `lsmod` trên BBB cho kết quả:

```
Module                  Size  Used by
snd_soc_simple_card    16384  0
snd_soc_simple_card_utils    24576  1 snd_soc_simple_card
ili9341                16384  1
drm_mipi_dbi           28672  1 ili9341
snd_soc_hdmi_codec     16384  0
snd_soc_core          212992  3 snd_soc_hdmi_codec,snd_soc_simple_card_utils,snd_soc_simple_card
snd_pcm_dmaengine      16384  1 snd_soc_core
snd_pcm               106496  3 snd_pcm_dmaengine,snd_soc_hdmi_codec,snd_soc_core
snd_timer              32768  1 snd_pcm
snd                    69632  4 snd_soc_hdmi_codec,snd_timer,snd_soc_core,snd_pcm
soundcore              16384  1 snd
sch_fq_codel           20480  2
fuse                  118784  1
configfs               40960  1
```

### 4.2. Phân tích: module nào cần, module nào không

Đối chiếu với yêu cầu hệ thống ở mục 1:

| Module | Chức năng | Cần giữ? |
|--------|-----------|----------|
| `ili9341` | Driver cho display ILI9341 qua SPI | **Có** — hiển thị UI |
| `drm_mipi_dbi` | Lớp DRM cho MIPI DBI interface, ili9341 phụ thuộc module này | **Có** — dependency của ili9341 |
| `snd_soc_simple_card` | ALSA sound card driver tổng quát | Không — không có audio |
| `snd_soc_simple_card_utils` | Utility cho simple_card | Không |
| `snd_soc_hdmi_codec` | Audio codec qua HDMI | Không — không dùng HDMI audio |
| `snd_soc_core` | ALSA SoC framework (~213KB) | Không |
| `snd_pcm_dmaengine` | DMA engine cho PCM audio | Không |
| `snd_pcm` | PCM audio subsystem (~106KB) | Không |
| `snd_timer` | Timer cho audio | Không |
| `snd` | ALSA core (~70KB) | Không |
| `soundcore` | Sound subsystem core | Không |
| `sch_fq_codel` | Network packet scheduler (Fair Queuing) | Tùy — nhỏ, kernel tự load |
| `fuse` | Filesystem in Userspace | Tùy — phụ thuộc có dùng FUSE mount không |
| `configfs` | Configuration filesystem | Tùy — device tree overlay có thể cần |

Toàn bộ nhóm `snd*` và `soundcore` chiếm khoảng **500KB** modules và không phục vụ chức năng nào của gateway (không có loa, micro, hay audio codec). Chúng được load vì AM335x default kernel config bật audio subsystem — SoC AM335x có McASP audio peripheral nên defconfig mặc định enable nó.

### 4.3. Tắt audio trong kernel config

Vì dự án không sử dụng audio, tắt toàn bộ sound subsystem trong kernel config fragment:

```
CONFIG_SOUND=n
CONFIG_SND=n
CONFIG_SND_SOC=n
```

| Config | Mô tả | Tác dụng |
|--------|-------|----------|
| `CONFIG_SOUND` | Top-level switch cho toàn bộ sound subsystem. | Tắt config này vô hiệu hóa mọi thứ audio bên dưới — không build bất kỳ sound module nào. |
| `CONFIG_SND` | ALSA driver framework — cung cấp `/dev/snd/*` devices. | Con của CONFIG_SOUND. Tắt tường minh để đảm bảo ngay cả khi CONFIG_SOUND bị miss. |
| `CONFIG_SND_SOC` | ALSA SoC layer — audio driver cho SoC (codec, I2S, McASP). | AM335x defconfig bật config này mặc định. Tắt để loại `snd_soc_core` (module lớn nhất, 213KB). |

Sau khi apply, `lsmod` sẽ không còn bất kỳ module `snd*` hay `soundcore` nào.

### 4.4. Mở rộng (nếu cần tối ưu sâu hơn)

Tương tự cách tiếp cận trên, có thể tiếp tục tắt các subsystem khác nếu xác nhận không dùng:

```
CONFIG_USB_STORAGE=n       # USB mass storage (nếu không dùng USB drive)
CONFIG_BT=n                # Bluetooth trong kernel
CONFIG_WIRELESS=n          # WiFi trong kernel
CONFIG_INPUT_TOUCHSCREEN=n # Touchscreen drivers (nếu không có touch panel)
```

Nguyên tắc: chạy `lsmod` → xác định module không cần → tìm CONFIG tương ứng → tắt trong kernel config fragment → build lại → chạy `lsmod` verify.

---

## 5. Tuyển chọn packages cho image

### 5.1. Production packages

Chỉ cài đúng những gì đã xác định ở mục 1.1:

```bitbake
IMAGE_INSTALL:append = " \
    openssh openssh-sshd \
    mosquitto libmosquitto1 mosquitto-clients \
    libgpiod-tools libgpiod \
    ttf-dejavu-sans fontconfig \
    qtbase \
    smarthome bbb-static-ip \
"
```

So với image recipe cũ, các package sau đã bị loại bỏ:

| Package | Mô tả | Lý do loại bỏ |
|---------|-------|---------------|
| `openssh-sftp-server` | SFTP subsystem — cho phép truyền file qua giao thức SFTP (dùng bởi FileZilla, WinSCP). | OTA update qua SWUpdate, không qua SFTP. SSH shell vẫn hoạt động. `scp` vẫn dùng được vì chạy qua SSH protocol trực tiếp, không phụ thuộc sftp-server. |
| `mosquitto-dev` | Header files (`.h`) và pkg-config files (`.pc`) của libmosquitto. Dùng khi **cross-compile** chương trình C/C++ gọi API mosquitto trên build host. | Trên target chỉ **chạy** binary đã compile xong. Binary link với `libmosquitto1.so` (runtime library, vẫn giữ). Header files không có tác dụng gì trên target. Lỗi phổ biến: nhầm `-dev` package (build-time dependency) với runtime library. |
| `kernel-modules` | Meta-package cài **toàn bộ** kernel modules được build ra — bao gồm GPU, SCSI, virtio, block, USB, watchdog... tất cả driver dạng loadable module. | Thay bằng chỉ 2 module thực sự cần (mục 5.3). Các module GPU, SCSI, virtio không tương ứng với hardware nào trên board. |

### 5.2. Development packages — cơ chế DEV_MODE

Thay vì trộn lẫn debug tools với runtime packages, tách riêng và kiểm soát bằng biến `DEVELOPMENT_BUILD`:

```bitbake
DEV_PACKAGES = " \
    i2c-tools \
    evtest \
    systemd-analyze \
    tslib tslib-calibrate tslib-tests \
"

DEV_MODE ?= "0"
IMAGE_INSTALL:append = " \
    ${@'${DEV_PACKAGES}' if d.getVar('DEVELOPMENT_BUILD') == '1' else ''} \
"
```

**Cách hoạt động:** Biến `DEVELOPMENT_BUILD` mặc định không tồn tại (hoặc khác "1"), nên biểu thức Python `d.getVar('DEVELOPMENT_BUILD') == '1'` trả về False → `DEV_PACKAGES` không được cài. Khi cần build image cho development, thêm vào `local.conf`:

```bitbake
DEVELOPMENT_BUILD = "1"
```

Khi build production image, bỏ dòng trên (hoặc set = "0").

**Lợi ích:**
- Một image recipe duy nhất phục vụ cả hai mục đích
- Không bao giờ vô tình ship debug tools lên production
- Dễ toggle qua CI/CD pipeline: truyền biến khác nhau cho dev build vs release build

**Danh sách dev packages và vai trò:**

| Package | Mô tả | Dùng khi nào |
|---------|-------|-------------|
| `i2c-tools` | CLI tools: `i2cdetect` (scan bus), `i2cget`/`i2cset` (đọc/ghi register). | Debug sensor I2C — xác nhận thiết bị có trên bus, đọc register kiểm tra giá trị. |
| `evtest` | Đọc và hiển thị Linux input events từ `/dev/input/eventX`. | Debug input devices — kiểm tra touchscreen, nút bấm, encoder gửi event đúng không. |
| `systemd-analyze` | Phân tích boot time: `systemd-analyze blame` liệt kê service theo thời gian khởi động. | Tối ưu boot time — tìm service chậm để cải thiện. |
| `tslib` + calibrate + tests | Thư viện touchscreen: `tslib` xử lý raw touch data, `tslib-calibrate` chạy calibration interactive, `tslib-tests` verify input. | Debug touchscreen (nếu có XPT2046 overlay trên ILI9341). |

### 5.3. Kernel modules — chọn lọc thay vì cài tất cả

```bitbake
IMAGE_INSTALL:append = " \
    kernel-module-ili9341 \
    kernel-module-drm-mipi-dbi \
"
```

| Module | Chức năng |
|--------|-----------|
| `kernel-module-ili9341` | Driver cho chip display ILI9341 — giao tiếp SPI, quản lý framebuffer, nhận lệnh vẽ từ DRM subsystem. |
| `kernel-module-drm-mipi-dbi` | Lớp trừu tượng DRM cho MIPI DBI interface — ili9341 driver đăng ký thông qua layer này để tích hợp với Linux display stack. |

So với `kernel-modules` (toàn bộ 6.5MB), chỉ 2 module này tiết kiệm ~5.5MB.

---

## 6. Loại bỏ locale data

```bitbake
IMAGE_LINGUAS = ""
```

`IMAGE_LINGUAS` quyết định locale và translation files nào được cài. Mặc định Yocto cài "en-us" cùng translation data cho nhiều package. Đặt rỗng = chỉ giữ POSIX/C locale, bỏ hết translation files.

Nếu ứng dụng `smarthome` hiển thị tiếng Việt, Qt tự xử lý qua QTranslator và file `.qm` đóng gói trong app, không phụ thuộc system locale.

---

## 7. Tổng kết

### 7.1. So sánh trước và sau

```
                        TRƯỚC                   SAU
Base image:        core-image-full-cmdline   core-image-base
DISTRO_FEATURES:   default (đầy đủ)          loại 16 features
Kernel config:     default (audio bật)       audio tắt
Packages:          15 packages               9 packages runtime
                   (lẫn dev + runtime)       + 4 dev packages (tắt ở prod)
Kernel modules:    tất cả (6.5MB)            2 modules (~1MB)
Locale:            default                   rỗng
Dev tools:         luôn cài                  toggle bằng DEVELOPMENT_BUILD
Rootfs ước tính:   385MB                     ~120MB
WIC ước tính:      1.1GB                     ~500MB
```

### 7.2. Impact theo từng tầng (cao đến thấp)

```
1. Chuyển base image                 -> ~60MB
   (full-cmdline -> base: loại Python, GIR, Vala,
    mc, bash, full coreutils, dependency chain)

2. DISTRO_FEATURES:remove            -> ~80–100MB
   (x11, opengl, gobject-introspection-data
    loại X11 libs, Mesa, Python bindings, GIR data)

3. Loại packages thừa                -> ~15MB
   (mosquitto-dev, openssh-sftp-server,
    kernel-modules full -> 2 modules chọn lọc)

4. Kernel config                     -> ~1MB
   (audio modules, đúng nguyên tắc dù nhỏ)

5. IMAGE_LINGUAS = ""                -> ~2–5MB
   (locale và translation data)
```