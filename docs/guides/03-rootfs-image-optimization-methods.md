# Phương pháp tối ưu rootfs

Tối ưu rootfs không phải chỉ là xóa bớt file cho nhẹ. Đó là quá trình trả lời một câu hỏi duy nhất:

> **"Cái này có chạy trên target không?"**

Embedded system khác desktop ở chỗ: desktop cài sẵn mọi thứ để tiện cho người dùng, embedded chỉ chứa đúng những gì cần để hoàn thành nhiệm vụ. Mỗi MB thừa trên rootfs đều có giá:
- Tăng thời gian OTA update (truyền qua mạng lâu hơn)
- Tăng kích thước partition → cần storage lớn hơn → tăng BOM cost
- Tăng thời gian boot (nhiều service hơn, nhiều file hơn để mount)

Nguyên tắc vàng: **bắt đầu từ tối thiểu, thêm vào** — không phải bắt đầu từ đầy đủ rồi cắt bớt. Tuy nhiên thực tế cũng có nhiều dự án bắt đầu bằng image lớn để phát triển nhanh, rồi tối ưu sau. Guide này cover cả hai hướng.

---

## 1. Xác định yêu cầu hệ thống

Trước khi chạm vào bất kỳ file config nào, trả lời ba câu hỏi sau.

### 1.1. Sản phẩm bật lên rồi làm gì?

Liệt kê từng chức năng runtime — chỉ những gì **thực sự chạy** trên board, không phải những gì "có thể cần sau này".

*Ví dụ — Smart Home Gateway:*

| Chức năng | Thành phần | Package tương ứng |
|-----------|------------|-------------------|
| Hiển thị UI trên ILI9341 | Qt5 linuxfb | `qtbase`, `ttf-dejavu-sans` |
| MQTT client | Mosquitto daemon | `mosquitto`, `libmosquitto1` |
| Remote access | SSH server | `openssh`, `openssh-sshd` |
| Điều khiển GPIO | libgpiod | `libgpiod`, `libgpiod-tools` |

*Ví dụ — IP Camera:*

| Chức năng | Thành phần | Package tương ứng |
|-----------|------------|-------------------|
| Video capture | V4L2 + ISP driver | `v4l-utils` |
| H.264 encode | Hardware encoder | `gstreamer1.0-plugins-*` |
| RTSP streaming | GStreamer RTSP server | `gstreamer1.0-rtsp-server` |
| Web config | Lighttpd | `lighttpd` |

### 1.2. Board giao tiếp với phần cứng nào?

Liệt kê từng interface vật lý và xác định cần kernel module hay built-in:

| Interface | Dùng cho | Module hay built-in? |
|-----------|----------|---------------------|
| SPI | Display, sensor | Thường cần module (spidev, driver cụ thể) |
| I2C | Sensor, EEPROM | Thường built-in, có thể cần i2c-dev module |
| UART | Debug console | Built-in |
| GPIO | Relay, LED, nút bấm | Built-in |
| Ethernet | Mạng | Built-in |
| USB | WiFi dongle, Zigbee stick | Cần module tương ứng |
| CAN | Giao tiếp công nghiệp | Cần module |
| MIPI CSI | Camera | Cần module |

### 1.3. Phần cứng và tính năng nào không có?

Đây là câu hỏi quan trọng nhất vì nó quyết định feature nào loại bỏ. Đi qua từng mục và đánh dấu:

```
□ Màn hình HDMI/LVDS           → có / KHÔNG CÓ
□ GPU                          → có / KHÔNG CÓ
□ Loa / micro / audio codec    → có / KHÔNG CÓ
□ WiFi (on-board hoặc dongle)  → có / KHÔNG CÓ
□ Bluetooth                    → có / KHÔNG CÓ
□ NFC                          → có / KHÔNG CÓ
□ Modem 3G/4G                  → có / KHÔNG CÓ
□ PCI/PCIe slot                → có / KHÔNG CÓ
□ PCMCIA slot                  → có / KHÔNG CÓ
□ Touchscreen                  → có / KHÔNG CÓ
□ Bàn phím / chuột vật lý      → có / KHÔNG CÓ
□ USB storage                  → có / KHÔNG CÓ
□ Camera                       → có / KHÔNG CÓ
```

Mỗi mục đánh dấu "KHÔNG CÓ" tương ứng với một hoặc nhiều `DISTRO_FEATURES` và kernel config cần tắt.

---

## 2. Ánh xạ yêu cầu phần cứng sang cấu hình build system

Từ checklist ở mục 1.3, tra bảng để xác định cần loại bỏ gì:

### 2.1. DISTRO_FEATURES

`DISTRO_FEATURES` quyết định tính năng nào được build **xuyên suốt toàn bộ hệ thống**. Khi một feature nằm trong biến này, mọi package hỗ trợ feature
đó sẽ bật nó lên. Loại bỏ ở đây có hiệu ứng lan truyền — một dòng remove có thể loại hàng chục packages.

| Phần cứng | Feature cần remove | Packages bị loại theo |
|--------------------|--------------------|-----------------------|
| GPU | `x11` | libX11, libxcb, xcb-util, xkeyboard-config, `/usr/share/X11` |
| GPU | `wayland` | wayland-client, weston, libwayland |
| GPU | `opengl` | mesa, libGL, libEGL, libGLES |
| GPU | `vulkan` | vulkan-loader, vulkan-headers |
| Loa / micro | `alsa` | alsa-lib, alsa-utils, alsa-state |
| Loa / micro | `pulseaudio` | pulseaudio daemon, libpulse |
| WiFi | `wifi` | wpa-supplicant, iw, wireless-regdb |
| Bluetooth | `bluetooth` | bluez5, obex, bluetooth firmware |
| NFC | `nfc` | neard |
| Modem 3G/4G | `3g` | ofono, ModemManager |
| Modem | `ppp` | ppp daemon |
| PCI/PCIe | `pci` | pciutils, libpci |
| PCMCIA | `pcmcia` | pcmciautils |
| Không dùng GObject bindings | `gobject-introspection-data` | python3, gir-1.0 data, vala |

Cách xem `DISTRO_FEATURES` hiện tại:

```bash
# Trên build host:
bitbake -e <image-name> | grep ^DISTRO_FEATURES=
```

Cách remove trong `local.conf` hoặc distro config:

```bitbake
DISTRO_FEATURES:remove = "x11 wayland opengl vulkan bluetooth wifi alsa"
```

### 2.2. Kernel config

Một số driver kernel vẫn được build dù đã remove `DISTRO_FEATURES`. Lý do: kernel config và `DISTRO_FEATURES` là hai hệ thống độc lập — `DISTRO_FEATURES` kiểm soát userspace, kernel config kiểm soát kernel space. Cần tắt ở cả hai.

| Phần cứng | Kernel config cần tắt |
|--------------------|-----------------------|
| Audio | `CONFIG_SOUND=n`, `CONFIG_SND=n`, `CONFIG_SND_SOC=n` |
| WiFi | `CONFIG_WIRELESS=n`, `CONFIG_CFG80211=n` |
| Bluetooth | `CONFIG_BT=n` |
| GPU | `CONFIG_DRM=n` (cẩn thận nếu display dùng DRM — xem ghi chú bên dưới) |
| USB storage | `CONFIG_USB_STORAGE=n` |
| Touchscreen | `CONFIG_INPUT_TOUCHSCREEN=n` |
| Camera | `CONFIG_MEDIA_SUPPORT=n` (nếu không dùng V4L2) |

**Ghi chú về DRM:** Một số SPI display (ILI9341, ST7789) dùng DRM MIPI DBI subsystem thay vì fbtft cũ. Nếu display driver phụ thuộc DRM, không tắt `CONFIG_DRM`. Kiểm tra bằng `lsmod | grep drm` trên board.

Cách kiểm tra module nào đang load để quyết định tắt config nào:

```bash
# Trên board:
lsmod                    # Liệt kê tất cả modules đang load
lsmod | grep snd         # Tìm audio modules
lsmod | grep drm         # Tìm display modules
lsmod | grep bt          # Tìm bluetooth modules
```

Cách tạo kernel config fragment trong Yocto:

```conf
# Tạo file .cfg trong layer, ví dụ:
# recipes-kernel/linux/files/disable-audio.cfg

CONFIG_SOUND=n
CONFIG_SND=n
CONFIG_SND_SOC=n
```

```bitbake
# Trong recipes-kernel/linux/linux-yocto_%.bbappend:
SRC_URI += "file://disable-audio.cfg"
```

---

## 3. Chọn base image phù hợp

Base image quyết định điểm xuất phát — càng nhẹ càng ít phải cắt.

| Image | Dung lượng | Bao gồm | Dùng cho |
|-------|-----------|---------|----------|
| `core-image-minimal` | ~8–15MB | Busybox, init system, base-files | Headless, single-purpose device |
| `core-image-base` | ~25–40MB | + udev, hardware support, networking | networking, systemd, general embedded |
| `core-image-full-cmdline` | ~150–300MB | + bash, coreutils, Python, mc, screen, dev tools | **Chỉ dùng development/lab**, không dùng cho production |
| `core-image-sato` | ~500MB+ | + X11, GTK, desktop apps | Embedded có desktop GUI |

**Nguyên tắc:** chọn image nhẹ nhất đáp ứng yêu cầu, thêm package manually.

`core-image-full-cmdline` là bẫy phổ biến: nó kéo `packagegroup-core-full-cmdline` bao gồm bash, full coreutils, mc, screen, tmux, và quan trọng nhất —
dependency chain dẫn tới Python, gobject-introspection, vala. Dùng nó làm base cho production image là nguồn gốc của hàng trăm MB thừa.

Cách xem base image kéo theo những package nào:

```bash
# Xem packagegroup contents:
bitbake -e packagegroup-core-full-cmdline | grep ^RDEPENDS=

# So sánh 2 image:
bitbake core-image-base && cp tmp/deploy/images/*/*.manifest /tmp/base.manifest
bitbake core-image-full-cmdline && cp tmp/deploy/images/*/*.manifest /tmp/full.manifest
comm -13 <(sort /tmp/base.manifest) <(sort /tmp/full.manifest)
# → liệt kê packages có trong full-cmdline mà không có trong base
```

---

## 4. Khảo sát dung lượng rootfs

Sau khi ta áp dụng `DISTRO_FEATURES` + kernel config + chọn base image, build image và đo dung lượng thực tế. Nếu vẫn lớn, cần khảo sát chi tiết.

### 4.1. Đo tổng quan

Chạy trên board hoặc mount rootfs trên build host:

**Tổng dung lượng rootfs**

```bash
du -sh /
```

**Top thư mục chiếm dung lượng**

```bash
du -sh /* | sort -rh | head -20
```

Kết quả thường thấy:

```
/usr    — lớn nhất (chứa lib, bin, share)
/lib    — kernel modules, system libraries
/boot   — kernel image, DTB (nếu không dùng partition riêng)
/etc    — config files (thường nhỏ)
```

Nếu `/usr` chiếm >70% tổng → đào sâu vào `/usr`.

**Đào sâu vào /usr**

```bash
du -sh /usr/* | sort -rh
du -sh /usr/lib/* | sort -rh | head -20
du -sh /usr/share/* | sort -rh | head -20
du -sh /usr/bin/* | sort -rh | head -20
```

**Kernel modules**

```bash
du -sh /lib/modules
ls /lib/modules/*/kernel/drivers/
lsmod    # xem module nào đang thực sự được load
```

So sánh: `ls` cho thấy tất cả module đã cài, `lsmod` cho thấy module đang dùng. Chênh lệch giữa hai danh sách = modules cắt được.

### 4.2. Đọc kết quả — nhận diện bất thường

Pattern bình thường cho embedded image tối ưu:

```
/usr/lib/    — thư viện .so -> chiếm nhiều nhất, bình thường
/usr/bin/    — binary files -> vài MB
/usr/share/  — data files -> nên nhỏ (<5MB)
/lib/modules — kernel modules -> nên nhỏ (<2MB)
/etc/        — config -> <3MB
```

Dấu hiệu bất thường — cần điều tra:

| Phát hiện | Có thể là | Kiểm tra bằng |
|-----------|-----------|---------------|
| `/usr/share/` > 20MB | locale, gir, vala, terminfo, man, X11, mime | `du -sh /usr/share/* \| sort -rh` |
| `/usr/lib/python*` tồn tại | Python bị kéo bởi dependency | `ls /usr/lib/python*/site-packages/` |
| `/usr/include/` tồn tại | Dev headers trên target | Xóa được |
| `/usr/lib/*.a` tồn tại | Static libraries trên target | Xóa được |
| `/lib/modules/` > 5MB | Cài quá nhiều kernel modules | `lsmod` so với `ls /lib/modules/*/` |
| `/usr/share/fonts/` > 3MB | Font đầy đủ thay vì subset | Kiểm tra font nào cần |

### 4.3. Xem danh sách packages trong image

Đọc file manifest từ build host, file này liệt kê tất cả packages trong image. Đây là danh sách chính xác nhất.

```bash
# File manifest liệt kê TẤT CẢ packages trong image
cat tmp/deploy/images/<machine>/<image-name>-<machine>.manifest

# Đếm số packages
wc -l tmp/deploy/images/<machine>/<image-name>-<machine>.manifest

# Sắp xếp theo tên để dễ đọc
sort tmp/deploy/images/<machine>/<image-name>-<machine>.manifest
```

Trên board (nếu có package manager):

```bash
# Yocto với opkg:
opkg list-installed

# Yocto với dpkg:
dpkg-query -W --showformat='${Installed-Size}\t${Package}\n' | sort -rn | head -30

# Yocto với rpm:
rpm -qa --queryformat '%{SIZE}\t%{NAME}\n' | sort -rn | head -30
```

---

## 5. Truy vết package — tìm nguồn gốc và dependency

Ta phát hiện một package lạ trên image và cần biết: nó là gì, ai kéo nó vào, và làm sao loại nó. Quy trình chung:

```
Phát hiện file/thư mục lạ trên rootfs
  → Tìm file đó thuộc package nào
    → Xác định package đó vào image bằng cách nào
      → Chọn cách loại bỏ phù hợp
```

### 5.1. Tìm package nào chứa file cụ thể

Trên board phát hiện thấy `/usr/share/gir-1.0` chiếm dung lượng -> File này thuộc package nào?

```bash
# Trên build host:
oe-pkgdata-util find-path /usr/share/gir-1.0
# Output: gobject-introspection

oe-pkgdata-util find-path /usr/lib/libQt5XcbQpa.so.5
# Output: qtbase-plugins
```

`oe-pkgdata-util` tra cứu package database của Yocto — ánh xạ từ đường dẫn file trên rootfs sang tên package.

### 5.2. Tìm package nào kéo một package vào image

Biết `python3` không nên có trên image, nhưng nó vẫn bị cài. Ai kéo nó?

```bash
# Cách 1: dùng oe-pkgdata-util
oe-pkgdata-util lookup-recipe python3
# → cho biết recipe nào tạo ra package python3

# Cách 2: dùng bitbake dependency graph
bitbake -g <image-name>
# Tạo ra file: task-depends.dot

# Tìm ai phụ thuộc python3:
grep "python3" task-depends.dot | head -20

# Cách 3: tìm RDEPENDS (runtime dependency) cụ thể
grep -r "RDEPENDS.*python3" tmp/pkgdata/*/runtime/
# → cho biết package nào khai báo python3 là dependency

# Cách 4: dùng depexp (dependency explorer) nếu đã build
bitbake -u depexp <image-name>
# Mở GUI hiển thị dependency tree
```

### 5.3. Xem PACKAGECONFIG của một package

Package lớn (Qt, systemd, gstreamer) có nhiều feature tùy chọn được kiểm soát bởi `PACKAGECONFIG`. Xem config hiện tại:

```bash
# Xem PACKAGECONFIG đang active cho qtbase:
bitbake -e qtbase | grep ^PACKAGECONFIG=

# Output ví dụ:
# PACKAGECONFIG="accessibility dbus fontconfig freetype glib gui
#   harfbuzz icu jpeg libinput libpng opengl openssl sql-sqlite
#   tslib udev widgets xcb xkbcommon xlib xrender"

# Xem tất cả PACKAGECONFIG có thể (kể cả đang tắt):
bitbake -e qtbase | grep ^PACKAGECONFIG
```

Từ output trên, xác định feature nào cần bỏ:

```bitbake
# Trong local.conf hoặc bbappend:
PACKAGECONFIG:remove:pn-qtbase = "xcb opengl x11 tslib sql-sqlite"
PACKAGECONFIG:append:pn-qtbase = " linuxfb"
```

Một số package phổ biến có PACKAGECONFIG đáng kiểm tra:

```bash
bitbake -e systemd | grep ^PACKAGECONFIG=
bitbake -e gstreamer1.0 | grep ^PACKAGECONFIG=
bitbake -e curl | grep ^PACKAGECONFIG=
bitbake -e openssh | grep ^PACKAGECONFIG=
```

### 5.4. Xem dependency tree đầy đủ của image

```bash
# Tạo dependency graph
bitbake -g <image-name>

# File output:
#   task-depends.dot    — đầy đủ nhất, bao gồm task-level dependency
#   recipe-depends.dot  — recipe-level dependency (dễ đọc hơn)

# Tìm chain: ai kéo python3 vào?
# Đọc recipe-depends.dot, tìm ngược từ python3:
grep '"python3"' recipe-depends.dot
# Output:
# "gobject-introspection" -> "python3"
# Tiếp tục tìm:
grep '"gobject-introspection"' recipe-depends.dot
# "glib-2.0" -> "gobject-introspection"
```

Cách đọc: `"A" -> "B"` nghĩa là A phụ thuộc B. Tìm ngược từ package
muốn loại → biết chain → cắt ở gốc (loại DISTRO_FEATURES hoặc
PACKAGECONFIG) thay vì cắt ở ngọn.

### 5.5. So sánh manifest trước và sau

```bash
# Lưu manifest trước khi thay đổi
cp tmp/deploy/images/<machine>/<image>*.manifest /tmp/before.manifest

# Build lại sau thay đổi
bitbake <image-name>

# So sánh
diff /tmp/before.manifest tmp/deploy/images/<machine>/<image>*.manifest

# Hoặc xem chi tiết hơn:
comm -23 <(sort /tmp/before.manifest) \
         <(sort tmp/deploy/images/<machine>/<image>*.manifest)
# → packages có trong bản cũ mà không có trong bản mới (đã bị loại)

comm -13 <(sort /tmp/before.manifest) \
         <(sort tmp/deploy/images/<machine>/<image>*.manifest)
# → packages mới xuất hiện (nếu có)
```

---

## 6. Các kỹ thuật loại bỏ packages

Sau khi xác định package cần loại, có nhiều cách loại tùy tình huống. Ưu tiên từ trên xuống — giải quyết ở tầng cao trước:

### 6.1. Loại qua DISTRO_FEATURES (ưu tiên cao nhất)

Hiệu ứng lan truyền mạnh nhất — ảnh hưởng toàn bộ build.

```bitbake
DISTRO_FEATURES:remove = "x11 opengl bluetooth wifi alsa"
```

Dùng khi: package bị kéo vào vì một feature hệ thống mà board không cần.

### 6.2. Loại qua PACKAGECONFIG

Ảnh hưởng một recipe cụ thể — tắt feature optional của package đó.

```bitbake
PACKAGECONFIG:remove:pn-qtbase = "xcb opengl"
PACKAGECONFIG:remove:pn-systemd = "backlight hibernate"
```

Dùng khi: package lớn (Qt, systemd, gstreamer) build nhiều sub-feature mà ta không cần.

### 6.3. Loại qua IMAGE_INSTALL

Trực tiếp nhất — không cài package vào image.

```bitbake
# Bỏ ra khỏi IMAGE_INSTALL
# Hoặc dùng IMAGE_INSTALL:remove nếu package bị thêm bởi layer khác:
IMAGE_INSTALL:remove = "packagegroup-core-full-cmdline"
```

Dùng khi: package được liệt kê tường minh trong image recipe.

### 6.4. Chặn qua BAD_RECOMMENDATIONS

Chặn package optional mà RRECOMMENDS kéo vào.

```bitbake
BAD_RECOMMENDATIONS += "shared-mime-info ncurses-terminfo"
```

Dùng khi: package không nằm trong IMAGE_INSTALL nhưng vẫn được cài vì package khác RRECOMMENDS nó.

Cách tìm ai recommends một package:

```bash
grep -r "RRECOMMENDS.*shared-mime-info" tmp/pkgdata/*/runtime/
```

Hoặc chặn toàn bộ recommendations:

```bitbake
NO_RECOMMENDATIONS = "1"
```

**Cẩn thận:** `NO_RECOMMENDATIONS = "1"` chặn hết, có thể gây thiếu package cần thiết mà được khai báo qua RRECOMMENDS thay vì RDEPENDS. Test kỹ sau khi bật.

### 6.5. Dọn dẹp qua ROOTFS_POSTPROCESS_COMMAND

Phương án cuối cùng — xóa file trực tiếp khỏi rootfs sau khi build.

```bitbake
rootfs_cleanup() {
    # Luôn an toàn để xóa:
    rm -rf ${IMAGE_ROOTFS}/usr/include        # header files
    find ${IMAGE_ROOTFS} -name "*.a" -delete   # static libraries
    find ${IMAGE_ROOTFS} -name "*.la" -delete  # libtool archives
    rm -rf ${IMAGE_ROOTFS}/usr/share/man       # man pages
    rm -rf ${IMAGE_ROOTFS}/usr/share/doc       # documentation
    rm -rf ${IMAGE_ROOTFS}/usr/share/info      # info pages
    rm -rf ${IMAGE_ROOTFS}/usr/share/gtk-doc   # GTK documentation
}
ROOTFS_POSTPROCESS_COMMAND += "rootfs_cleanup; "
```

**Hạn chế:** không giải quyết gốc vấn đề (package vẫn bị build và cài, chỉ bị xóa sau). Dependency chain vẫn tồn tại. Ưu tiên giải quyết ở tầng 6.1–6.4 trước.

---

## 10. Kỹ thuật nâng cao

Chỉ cần khi bước 1–9 chưa đủ hoặc target rất hạn chế về storage.

### 10.1. Thay thế package nặng bằng bản nhẹ

| Nặng | Nhẹ | Tiết kiệm | Lưu ý |
|------|-----|-----------|-------|
| openssh (~3MB) | dropbear (~100KB) | ~2.9MB | Dropbear không hỗ trợ SFTP |
| bash (~2MB) | busybox sh (built-in) | ~2MB | Một số script cần sửa shebang |
| systemd (~15MB) | busybox init + mdev | ~15MB | Mất journal, timer, socket activation |
| glibc (~10MB) | musl (~1MB) | ~9MB | Compatibility issues với một số thư viện |
| curl (~2MB) | wget (busybox) | ~2MB | Mất HTTPS nếu busybox không build với TLS |

### 10.2. Read-only rootfs + SquashFS

```bitbake
IMAGE_FEATURES += "read-only-rootfs"
IMAGE_FSTYPES = "squashfs"
```

SquashFS nén rootfs 50–70%. Rootfs 120MB ext4 → ~50MB squashfs. Kết hợp overlayfs cho writable areas (`/var`, `/tmp`, `/data`). Phù hợp tuyệt vời với A/B OTA — mỗi slot nhỏ hơn đáng kể.

### 10.3. Compiler optimization for size

```bitbake
# Optimize for size thay vì speed
FULL_OPTIMIZATION = "-Os -pipe ${DEBUG_FLAGS}"
```

Giảm ~10–15% kích thước binary. Trade-off: performance giảm nhẹ (thường không đáng kể trên ứng dụng I/O-bound).

### 10.4. Kernel modules compression

```bitbake
MODULE_COMPRESS = "gz"
```

Nén kernel modules bằng gzip — giảm ~30% dung lượng `/lib/modules/`.