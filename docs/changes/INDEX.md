# Changes — INDEX

Log các thay đổi lớn kèm giải thích ("cái gì đã thay đổi và vì sao?").

## Danh sách tài liệu

| File | Nội dung |
|---|---|
| [0001-optimize-image.md](0001-optimize-image.md) | Tối ưu rootfs từ 385MB → ~120MB, chi tiết từng thay đổi |
| [0002-kernel-dtb-ab-via-ext4load.md](0002-kernel-dtb-ab-via-ext4load.md) | A/B toàn slot (kernel + dtb + rootfs) qua `ext4load`, bỏ FAT `/boot` |
| [0003-mqtt-broker-tren-host.md](0003-mqtt-broker-tren-host.md) | Chuyển MQTT broker từ BBB sang máy host, BBB chỉ còn là client |
| [0004-pwm-backlight-control.md](0004-pwm-backlight-control.md) | Qt app — điều khiển backlight TFT qua PWM |
| [0005-ota-pull-model-host-server.md](0005-ota-pull-model-host-server.md) | Thay đổi cơ chế update OTA |
| [0006-read-only-rootfs.md](0006-read-only-rootfs.md) | Rootfs slot A/B mount read-only |
| [0007-uboot-verified-boot.md](0007-uboot-verified-boot.md) | U-Boot Verified Boot: FIT image + RSA2048, feature `secure-boot` bật/tắt qua DISTRO_FEATURES |
| [0008-fix-fitimage-dtb-config.md](0008-fix-fitimage-dtb-config.md) | Fix FIT image dùng sai DTB |
| [0009-runtime-config-to-json.md](0009-runtime-config-to-json.md) | Chuyển env var sang JSON config file trên /data; thêm MqttSettings, mở rộng OtaSettings, auto-polling; sửa IP sang Ubuntu VM 192.168.137.10 |
| [0010-data-partition-build-time.md](0010-data-partition-build-time.md) | Build /data partition ext4 tại build time bằng mke2fs -d, nhúng config sẵn + data.mount unit |
| [0011-app-settings-json.md](0011-app-settings-json.md) | Thêm setting.json + class AppSettings lưu cấu hình home-dashboard (độ sáng), tồn tại qua reboot/OTA |
| [0012-dm-verity-rootfs.md](0012-dm-verity-rootfs.md) | dm-verity rootfs: root hash bake trong initramfs ký FIT, layout mở rộng 3→5 partition GPT (bootA/rootA/bootB/rootB/data) |

## Phân nhóm theo chủ đề

### Image / OTA / Bảo mật

- [0001-optimize-image.md](0001-optimize-image.md)
- [0002-kernel-dtb-ab-via-ext4load.md](0002-kernel-dtb-ab-via-ext4load.md)
- [0005-ota-pull-model-host-server.md](0005-ota-pull-model-host-server.md)
- [0006-read-only-rootfs.md](0006-read-only-rootfs.md)
- [0007-uboot-verified-boot.md](0007-uboot-verified-boot.md)
- [0008-fix-fitimage-dtb-config.md](0008-fix-fitimage-dtb-config.md)
- [0012-dm-verity-rootfs.md](0012-dm-verity-rootfs.md)

### MQTT / kết nối mạng / config

- [0003-mqtt-broker-tren-host.md](0003-mqtt-broker-tren-host.md)
- [0009-runtime-config-to-json.md](0009-runtime-config-to-json.md)
- [0010-data-partition-build-time.md](0010-data-partition-build-time.md)

### Qt application

- [0004-pwm-backlight-control.md](0004-pwm-backlight-control.md)
- [0005-ota-pull-model-host-server.md](0005-ota-pull-model-host-server.md)
- [0011-app-settings-json.md](0011-app-settings-json.md)
