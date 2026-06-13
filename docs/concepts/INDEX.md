# Concepts — INDEX

Khái niệm cốt lõi ("X là gì? hoạt động ra sao?").

## Danh sách tài liệu

| File | Nội dung |
|---|---|
| [01-ota-ab-architecture.md](01-ota-ab-architecture.md) | Cơ chế A/B update + rollback: slot A/B, biến trạng thái U-Boot env, luồng hoạt động |
| [02-read-only-rootfs.md](02-read-only-rootfs.md) | Read-only rootfs: nền tảng filesystem, tmpfs/OverlayFS/bind mount, phân loại dữ liệu runtime, triển khai trong Yocto |
| [03-secure-boot.md](03-secure-boot.md) | Secure Boot: U-Boot Verified Boot (FIT image + RSA2048) + dm-verity rootfs (root hash trong initramfs ký FIT), chuỗi tin cậy, bật/tắt theo DISTRO_FEATURES |

## Phân nhóm theo chủ đề

### OTA / A/B update

- [01-ota-ab-architecture.md](01-ota-ab-architecture.md)

### Image / rootfs

- [02-read-only-rootfs.md](02-read-only-rootfs.md)

### Bảo mật

- [03-secure-boot.md](03-secure-boot.md)
