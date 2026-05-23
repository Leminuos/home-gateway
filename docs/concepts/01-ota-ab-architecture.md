# Cơ chế A/B update

## A/B update là gì?

- Thiết bị có **2 slot hệ thống giống nhau**: `A` và `B`, mỗi slot là một bản hệ điều hành đầy đủ.
- Tại một thời điểm chỉ **một slot active** (đang chạy); slot kia **inactive** (dự phòng).
- Khi update: ghi firmware mới vào **slot inactive**, rồi chuyển sang boot slot đó. Slot đang chạy **không bị đụng tới**.
- Nếu slot mới boot fail → **rollback**: quay lại slot cũ vẫn còn nguyên vẹn.

> Mục tiêu: update không bao giờ làm "chết" thiết bị — luôn có một slot tốt để quay về.

---

## Vì sao cần A/B?

- **An toàn khi mất điện / update lỗi:** đang ghi mà mất điện thì chỉ hỏng slot inactive, slot active vẫn boot được.
- **Rollback tự động:** firmware mới crash, không lên được hệ thống → tự về bản cũ, không cần người can thiệp.
- **Swap atomic:** chỉ "lật" sang slot mới khi đã ghi xong toàn bộ — không có trạng thái nửa vời.

---

## Các khái niệm trạng thái

Để bootloader và hệ thống phối hợp được, cần lưu vài thông tin trạng thái (đặt ở vùng bền vững, cả bootloader lẫn userspace đều đọc/ghi được):

- **Slot active** — slot nào đang được chọn để boot.
- **Upgrade flag** — đánh dấu firmware vừa update, chưa được xác nhận.
- **Boot counter** — đếm số lần đã thử boot slot mới.
- **Boot limit** — số lần boot fail tối đa trước khi quay về slot cũ.

---

## Luồng hoạt động

```
   firmware mới
        │ ghi vào SLOT INACTIVE
        ▼
   đánh dấu: slot active = slot mới, cờ "đang thử" = bật
        │
        ▼
     reboot
        │
        ▼
   ┌──────────────────────────────┐
   │ Bootloader                    │
   │  thấy cờ upgrade -> đếm++     │
   └───────────────┬──────────────┘
                   │
   đếm < ngưỡng    │   đếm ≥ ngưỡng
       ┌───────────┴────────────┐
       ▼                        ▼
  boot slot mới            ROLLBACK
       │                   slot active = slot cũ
  boot OK?                 xóa cờ, reset đếm
   ┌────┴─────┐                 │
   ▼          ▼                 ▼
 chạy OK   fail / treo     boot lại slot cũ
   │          │              (bản ổn định)
   ▼          └──► reboot, thử lại
 xác nhận tốt      (quay lại bootloader)
 xóa cờ, reset đếm
   │
   ▼
 ✅ commit slot mới
```

---

## Các trạng thái chính

- **STABLE** — không có cờ upgrading. Hệ thống chạy ổn định trên slot active. Vừa là điểm xuất phát, vừa là đích đến.
- **TRIAL / UPGRADING** — cờ upgrading bật. Vừa flash xong, đang boot thử; mỗi lần boot tăng bộ đếm.
- **Kết quả:**
  - Hệ thống lên đầy đủ và tự **xác nhận tốt** → **COMMIT** → về STABLE trên slot mới.
  - Boot fail đủ số lần ngưỡng → **ROLLBACK** → về STABLE trên slot cũ.

---

## Rollback xảy ra khi nào?

- Slot mới **không boot được** (kernel panic, treo, rootfs hỏng) → bootloader đếm đủ ngưỡng rồi tự lật về slot cũ.
- Slot mới boot lên nhưng **không tự xác nhận** (service then chốt không chạy) → cờ "đang thử" vẫn còn → lần boot sau lại đếm tiếp → cuối cùng rollback.
- Điểm mấu chốt: **chỉ khi bản mới tự chứng minh là tốt** thì cờ mới được xóa; nếu không, hệ thống mặc định coi như fail và quay về.

---

## Liên quan

- Cách dự án triển khai cụ thể: [meta-ota/README.md](../../meta-ota/README.md)
- Vì sao A/B cả kernel/dtb chứ không chỉ rootfs: [decisions/01-rootfs-only-vs-full-ab.md](../decisions/01-rootfs-only-vs-full-ab.md)
