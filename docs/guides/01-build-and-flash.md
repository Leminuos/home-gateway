# Build image và flash SD card lần đầu

Tài liệu này hướng dẫn từng bước để có firmware chạy trên BeagleBone Black.

---

## 1. Chuẩn bị build host

Khuyến nghị Ubuntu 20.04/22.04. Cài các dependencies theo [Yocto Manual](https://docs.yoctoproject.org/kirkstone/ref-manual/system-requirements.html):

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 \
    xterm python3-subunit mesa-common-dev zstd liblz4-tool file locales
sudo locale-gen en_US.UTF-8
```

Cần ít nhất ~50GB dung lượng trống cho thư mục build dir và RAM tối thiểu 8GB (khuyến nghi 16GB).

---

## 2. Clone các layer phụ thuộc

Repo này (`meta-smartfarm`) không chứa Poky hoặc các meta-layer phụ thuộc, cần phải clone:

```bash
mkdir ~/yocto && cd ~/yocto

# Poky + meta-openembedded + meta-qt5 + meta-swupdate (Kirkstone branch)
git clone -b kirkstone https://git.yoctoproject.org/poky
git clone -b kirkstone https://git.openembedded.org/meta-openembedded
git clone -b kirkstone https://github.com/meta-qt5/meta-qt5.git
git clone -b kirkstone https://github.com/sbabic/meta-swupdate.git

# Repo dự án
git clone <repo-url> meta-smartfarm
```

Cấu trúc cuối:

```
~/yocto/
├── poky/                # Bitbake + core
├── meta-openembedded/   # core, networking, multimedia, python layers
├── meta-qt5/
├── meta-swupdate/
└── meta-smartfarm/      # repo dự án (meta-bsp + meta-ota)
```

---

## 3. Khởi tạo build directory

```bash
cd ~/yocto
source poky/oe-init-build-env build
```

Thư mục `build/` được tạo. Tất cả lệnh `bitbake` sau đây chạy từ đây.

---

## 4. Cấu hình `bblayers.conf`

Edit `conf/bblayers.conf`:

```bitbake
BBPATH = "${TOPDIR}"
BBFILES ?= ""

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
  /home/<user>/yocto/meta-smartfarm/meta-bsp \
  /home/<user>/yocto/meta-smartfarm/meta-ota \
"
```

Thứ tự `meta-bsp` trước `meta-ota` rất quan trọng vì `meta-ota` phụ thuộc vào `meta-bsp`.

---

## 5. Cấu hình `local.conf`

Edit `conf/local.conf`, set `MACHINE` và `DISTRO`:

```bitbake
MACHINE = "bbb-home-gateway"
DISTRO  = "home-gateway"
```

Optional — bật dev tools:

```bitbake
DEVELOPMENT_BUILD = "1"   # thêm i2c-tools, evtest, tslib-tests, systemd-analyze
```

Optional — override OTA version:

```bitbake
OTA_SW_VERSION = "0.1.0"
```

Optional — tăng tốc độ build nếu host có nhiều core:

```bitbake
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"
```

---

## 6. Build image

```bash
bitbake core-image-home-gateway
```

Lần đầu mất 2–4 tiếng (fetch + build toàn bộ toolchain + kernel + Qt). Các lần sau dùng sstate-cache, chỉ tốn vài phút nếu chỉ có vài thay đổi nhỏ.

Sau khi xong, các artifact nằm ở thư mục `tmp/deploy/images/bbb-home-gateway/`:

```
core-image-home-gateway-bbb-home-gateway.wic       ◄── flash file (toàn bộ SD)
core-image-home-gateway-bbb-home-gateway.wic.bmap  ◄── bmap để bmaptool flash nhanh hơn
core-image-home-gateway-bbb-home-gateway.ext4.gz   ◄── rootfs payload cho .swu
MLO, u-boot.img, u-boot-env.raw                    ◄── bootloader artifacts
zImage, am335x-boneblack.dtb                       ◄── kernel + DTB
boot.scr                                           ◄── compiled bootscript
```

---

## 7. Flash SD card

Cắm SD card vào host. Xác định device **(CỰC KỲ QUAN TRỌNG — flash nhầm sang ổ cứng host là mất data)**:

```bash
lsblk
# Tìm SD card — thường là /dev/sdb, /dev/sdc, hoặc /dev/mmcblk0 (nếu host có khe SD)
```

Để an toàn, nên verify lại bằng cách rút SD ra, chạy `lsblk` lần nữa, cắm vào, chạy lại — device nào mới xuất hiện chính là SD.

Unmount mọi partition đã auto-mount:

```bash
sudo umount /dev/sdX*    # X = ký tự device đã xác định ở trên
```

### Cách 1 — `dd` (đơn giản, chậm)

```bash
cd ~/yocto/build/tmp/deploy/images/bbb-home-gateway

sudo dd if=core-image-home-gateway-bbb-home-gateway.wic \
        of=/dev/sdX \
        bs=4M conv=fsync status=progress
sync
```

### Cách 2 — `bmaptool` (nhanh hơn ~3 lần, recommend)

```bash
cd ~/yocto/build/tmp/deploy/images/bbb-home-gateway

sudo bmaptool copy \
    core-image-home-gateway-bbb-home-gateway.wic \
    /dev/sdX
```

`bmaptool` đọc file `.wic.bmap` đi kèm, chỉ ghi các block thực sự có dữ liệu — skip vùng empty của p3 (rootB) và p4 (data).

Sau khi xong, eject:

```bash
sudo eject /dev/sdX
```

---

## 8. Boot lần đầu

1. Cắm SD card vào BeagleBone Black.
2. **Giữ nút S2** (nút "USER" gần SD slot) khi cấp nguồn — BBB sẽ ưu tiên boot từ SD thay vì eMMC.
3. Cắm serial (UART) qua FTDI cable hoặc nguồn USB:
   - UART pin: J1 (header chân 5V) — `GND` (pin 1), `RX` (pin 4), `TX` (pin 5).
   - Baud: 115200, 8N1.
4. Cấp nguồn (5V DC qua jack).

Console sẽ in ra log U-Boot, sau đó là log kernel + systemd. Khi đến `bbb-home-gateway login:` là boot xong.

Login mặc định:
- User: `root`
- Password: (trống — image lab/internal không set password)