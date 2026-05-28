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

## Phân nhóm theo chủ đề

### Image / OTA

- [0001-optimize-image.md](0001-optimize-image.md)
- [0002-kernel-dtb-ab-via-ext4load.md](0002-kernel-dtb-ab-via-ext4load.md)
- [0005-ota-pull-model-host-server.md](0005-ota-pull-model-host-server.md)
- [0006-read-only-rootfs.md](0006-read-only-rootfs.md)

### MQTT / kết nối mạng

- [0003-mqtt-broker-tren-host.md](0003-mqtt-broker-tren-host.md)

### Qt application

- [0004-pwm-backlight-control.md](0004-pwm-backlight-control.md)
- [0005-ota-pull-model-host-server.md](0005-ota-pull-model-host-server.md)
