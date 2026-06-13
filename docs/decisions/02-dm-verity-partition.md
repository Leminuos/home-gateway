# Quyết định: dm-verity với root hash trong initramfs ký FIT + layout 5 partition GPT

**Trạng thái:** Đã chốt (2026-06-10). Quyết định này **đảo ngược** `docs/decisions/01-rootfs-only-vs-full-ab.md` ở phần "kernel + dtb nằm trong rootfs, 3 partition" — nay tách kernel ra boot partition riêng, layout 5 partition.

**Bối cảnh:** Triển khai dm-verity để chống sửa rootfs trên block device (giới hạn đã ghi trong `docs/concepts/03-secure-boot.md`). Câu hỏi trung tâm: **đặt root hash của verity ở đâu để nó nằm trong chuỗi tin cậy đã ký?**

---

## Các phương án đã cân nhắc

### A. Root hash lưu trong U-Boot env

Build sinh root hash -> ghi vào biến env (`verity_a`/`verity_b`) trong `u-boot-env.raw`; U-Boot dựng bootargs `dm-mod.create=` cho kernel tự tạo verity device, không cần initramfs.

| Ưu | Nhược |
|---|---|
| Không cần initramfs, không sửa FIT, giữ nguyên 3 partition | **U-Boot env raw không được ký.** Kẻ tấn công ghi được env thì thay luôn cả rootfs lẫn root hash → dm-verity mất ý nghĩa chống tamper chủ động. Chỉ còn chống hỏng dữ liệu ngẫu nhiên. |
| Thay đổi tối thiểu | Quoting `dm-mod.create` (chuỗi nhiều dấu cách/phẩy) trong env + C header rất dễ sai |

### B. Giữ fitImage trong rootfs, root hash trong initramfs

Initramfs (chứa root hash) đóng trong fitImage; fitImage vẫn nằm tại `/boot/fitImage` trong rootfs như layout 3 partition cũ -> vòng lặp hash không giải được

**Vì sao vòng lặp này không giải được — đây là lý do cốt lõi loại phương án B:**

dm-verity tính `root hash = hash(toàn bộ nội dung rootfs)`. Nếu fitImage nằm *trong* rootfs (tại `/boot/fitImage`), thì fitImage là một phần nội dung rootfs. Mà fitImage lại chứa initramfs, initramfs chứa `/etc/verity.env` = root hash. Khép vòng:

```
root hash = hash(rootfs)
              │
              ├─ rootfs chứa /boot/fitImage   (fitImage là input của hash)
              │                    │
              │                    └─ fitImage chứa initramfs
              │                                   │
              │                                   └─ initramfs chứa root hash
              ▼                                              ▲
        để tính root hash, cần biết           nhưng để dựng fitImage,
        nội dung cuối của fitImage  ──────►   cần biết trước root hash
```

Tức là: muốn tính `root hash` thì phải có fitImage hoàn chỉnh (vì nó nằm trong rootfs được băm); nhưng muốn dựng fitImage thì phải có sẵn `root hash` (để nhồi vào initramfs). A cần B, B cần A — không cái nào tính trước được.

Về bản chất toán học, đây là bài toán tìm nội dung rootfs `X` sao cho `hash(X) = h` trong khi `h` lại được nhúng bên trong chính `X` — một dạng tìm điểm bất động của hàm băm, bất khả thi về mặt tính toán (nếu giải được thì hàm băm đã bị phá vỡ).

**Phân biệt với vòng lặp build của bitbake:** ngoài vòng lặp toán học trên, còn một vòng lặp *thứ tự build* (rootfs → root hash → initramfs → fitImage → lại vào rootfs). Vòng lặp build có thể phá bằng cách tách recipe; nhưng vòng lặp toán học thì **không** — chừng nào fitImage còn nằm trong vùng được băm thì không có thứ tự build nào cứu được. Đây chính là thứ buộc fitImage phải ra khỏi rootfs (phương án C).

### C. Root hash trong initramfs ký FIT, fitImage ra boot partition riêng

fitImage đặt ở boot partition ngoài vùng verity. Root hash nằm trong initramfs -> nằm trong FIT đã ký RSA -> chuỗi kín.

| Ưu | Nhược |
|---|---|
| **Chuỗi tin cậy kín và đã ký:** U-Boot pubkey -> FIT (ký) -> initramfs -> root hash -> block rootfs. | Layout phức tạp hơn: 5 partition, cần GPT (MBR chỉ 4 primary). |
| Tách bạch boot/root đúng chuẩn A/B đầy đủ | Đĩa breaking change — thẻ layout cũ phải reflash, không OTA chuyển layout được |
| Initramfs là điểm mở rộng tự nhiên cho các bước boot bảo mật sau này | Thêm initramfs (~4-5MB) vào FIT; thêm recipe lắp đĩa |

---

## Quyết định: chọn C

**Lý do chính:** mục tiêu của dm-verity là chống **sửa đổi chủ động** rootfs. Chỉ phương án C đặt root hash vào vùng đã ký (FIT) — A để hash ở env không ký nên tự phá mục tiêu, B vướng vòng lặp toán học không giải được. Cái giá của C (5 partition + GPT + initramfs) là chấp nhận được và một lần.

**Why:** Bảo mật của verity = bảo mật của nơi giữ root hash. Đặt root hash ngoài chuỗi ký = verity chỉ còn tác dụng chống bit-rot, không chống attacker — không đáng để triển khai.

**How to apply:** Mọi thay đổi tương lai phải giữ invariant: **root hash luôn nằm trong artifact đã ký RSA (FIT)**, và **fitImage luôn nằm ngoài vùng dữ liệu mà verity bảo vệ**. Không đưa root hash xuống env/data partition. Invariant atomic per-slot của decision 01 vẫn giữ: bootX + rootX là một cặp, OTA ghi cả hai cho slot đích, rollback về slot X = mọi artifact boot đều của slot X.

---

## Hệ quả

- Layout đĩa: 5 partition GPT thay 3 partition msdos. Chi tiết: `docs/concepts/01-ota-ab-architecture.md`.
- U-Boot: `fatload` từ boot partition FAT thay `ext4load` từ rootfs; cần `CONFIG_EFI_PARTITION` + `CONFIG_CMD_FAT`.
- Build: thêm recipe `home-gateway-disk` lắp đĩa; `core-image-home-gateway` chỉ sinh rootfs.
- OTA: mỗi slot ghi 2 image (boot + root).

## Khi nào nên đảo ngược / nâng cấp tiếp

- Nếu cần chống cả attacker thay U-Boot/MLO trên MMC -> phải dùng AM335x Secure Boot ở ROM (fusing eFuse), ngoài phạm vi hiện tại.
- Nếu sau này cần giảm số partition (vd ràng buộc dung lượng) mà vẫn giữ verity → cân nhắc nested container (rootfs verity là file trong ext4 outer cùng fitImage), nhưng phức tạp hơn về build.

## Tham chiếu

- Change log triển khai: [docs/changes/0012-dm-verity-rootfs.md](../changes/0012-dm-verity-rootfs.md)
- Quyết định bị đảo ngược: [docs/decisions/01-rootfs-only-vs-full-ab.md](01-rootfs-only-vs-full-ab.md)
