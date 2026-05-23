# Quyết định: A/B toàn bộ slot (kernel + dtb + rootfs) thay vì chỉ A/B rootfs

**Trạng thái:** Đã chốt (cập nhật 2026-05-20). Trước đây dự án chỉ A/B rootfs, kernel + dtb shared trên `/boot` FAT — quyết định này đảo ngược lựa chọn cũ.

**Bối cảnh:** OTA cần update được cả kernel/dtb giúp vá driver, đổi devicetree cho hardware mới. Layout cũ (kernel shared) không update kernel an toàn được.

---

## Các phương án đã cân nhắc

### A. Update kernel/dtb in-place trên `/boot` FAT (vẫn shared, không A/B kernel)

`sw-description` thêm entries ghi `zImage` + `*.dtb` vào partition p1 (FAT). Layout wks giữ nguyên 4 partition.

| Ưu | Nhược |
|---|---|
| Thay đổi tối thiểu — chỉ thêm vài entries vào `sw-description` | **Mất tính atomic.** Rollback rootfs về slot A nhưng kernel đã là bản mới → mismatch. Kernel mới có thể không tương thích userspace cũ (ABI module, syscall mới, devicetree binding). |
| Không tốn thêm dung lượng | **Brick rủi ro cao.** Mất điện giữa lúc ghi `zImage` lên FAT = board không boot được. FAT không có journal. |
| | Kernel + dtb cũ bị xóa khỏi `/boot` → không còn cách rollback kernel mà không reflash SD. |

### B. Kernel + dtb đặt trong rootfs, U-Boot đọc qua `ext4load` (chọn)

Bỏ partition `/boot` FAT hoàn toàn. Kernel + dtb nằm tại `/boot/zImage`, `/boot/*.dtb` trong **mỗi** rootfs slot (default Yocto đã đặt ở đây). U-Boot built-in env xác định active slot rồi `ext4load mmc 0:${mmc_part} ${kernel_addr_r} /boot/zImage`. Boot logic (rollback, count) cũng đưa vào built-in env, không còn `boot.scr` rời.

| Ưu | Nhược |
|---|---|
| **Atomic per-slot tuyệt đối.** Rollback về A = kernel A + dtb A + rootfs A đồng bộ, không bao giờ mismatch. | Cần U-Boot có ext4 support — am335x_evm defconfig đã bật sẵn `CONFIG_FS_EXT4`/`CONFIG_CMD_EXT4`. |
| Layout đơn giản hơn (3 partition thay vì 4). Tiết kiệm 16MB. | Logic boot bake vào U-Boot binary → muốn sửa logic A/B phải rebuild + reflash U-Boot raw (không OTA được). Trade-off chấp nhận được: logic này rất hiếm thay đổi. |
| Logic boot ở built-in env có CRC bảo vệ qua libubootenv. Không có file `boot.scr` rời cần đảm bảo có mặt + integrity. | Cắm SD vào host Windows/Mac không đọc được kernel trực tiếp (ext4 native). Phải dùng Linux host. Trade-off OK vì dev team dùng Linux. |
| Match với patterns chuẩn của `meta-swupdate` cho BBB. | Số partition shift: rootA `p2→p1`, rootB `p3→p2`, data `p4→p3` — phải đồng bộ sw-description, fstab, bootargs. |

### C. Thêm partition `bootA`/`bootB` FAT (full A/B explicit)

Wks 5 partition: `bootA | bootB | rootA | rootB | data`. U-Boot env chọn boot partition theo `active_slot`.

| Ưu | Nhược |
|---|---|
| Atomic per-slot | Tốn thêm ~16MB cho 1 partition phụ |
| Vẫn dùng FAT (host Windows đọc được) | wks + sw-description phức tạp hơn cả B, không lợi gì hơn về architecture |
| | Vẫn cần boot.scr riêng, vẫn có file lơ lửng ngoài rootfs cần lo integrity |

---

## Quyết định: chọn B

**Lý do chính:** atomic per-slot là yêu cầu cứng cho rollback đáng tin cậy. A bị loại vì phá rollback. B vs C cả hai đều atomic, nhưng B sạch hơn về layout (3 partition), không có file rời cần bảo vệ, và bake luôn được boot logic vào U-Boot env (đỡ thêm file `boot.scr`).

**Why:** Nếu kernel/dtb không A/B, OTA update kernel = trade rollback safety lấy convenience. Với mục tiêu OTA của dự án (production-grade rollback), đây là trade-off không chấp nhận được.

**How to apply:** Mọi thay đổi tương lai liên quan đến partition layout, kernel install path, hoặc U-Boot boot logic phải giữ nguyên invariant: **"rollback về slot X = mọi artifact boot đều là của slot X"**. Không thêm artifact shared (như boot.scr ở /data, kernel ở FAT chung) phá invariant này.

---

## Hệ quả

- Partition layout: 3 partition ext4 thay vì 1 FAT + 3 ext4. Chi tiết: [docs/concepts/01-ota-ab-architecture.md](../concepts/01-ota-ab-architecture.md) section 2.
- Boot logic chuyển từ `boot.scr` (compile bằng `mkimage` từ `boot.cmd`) sang built-in U-Boot env qua patch `am335x_evm.h`.
- Kernel + dtb không còn deploy ra `IMAGE_BOOT_FILES` — để Yocto default install vào rootfs `/boot/`.
- Update logic A/B = phải rebuild + reflash U-Boot raw (MLO + u-boot.img). Trade-off đã chấp nhận: rất hiếm thay đổi.

## Khi nào nên đảo ngược lại quyết định này

- Nếu cần debug bằng cách swap kernel manually từ host Windows/Mac mà không có máy Linux → quay lại layout có FAT.
- Nếu logic boot cần thay đổi thường xuyên (ví dụ thêm telemetry, network boot fallback) → tách `boot.scr` ra để OTA được riêng phần logic.

## Tham chiếu

- Change log triển khai cụ thể: [docs/changes/0002-kernel-dtb-ab-via-ext4load.md](../changes/0002-kernel-dtb-ab-via-ext4load.md)
