# Read-only rootfs cho slot A/B

**Ngày:** 2026-05-26
**Loại:** Hardening image — rootfs slot active mount read-only, tách rõ vùng ghi bền vững.

---

## 1. Mục tiêu

Slot rootfs đang chạy phải **immutable**, tức là không có process nào sửa được nó lúc runtime. Lý do:
- **Khớp mô hình A/B:** rollback chỉ an toàn nếu slot là bản build cố định. Nếu runtime ghi lung tung vào rootfs (log, file tạm, config,...) thì slot A lúc rollback không còn giống lúc flash — mất tính tái lập.
- **Chống hỏng filesystem khi mất điện:** rootfs không bao giờ ở trạng thái đang ghi -> mất điện không làm hỏng ext4 của slot. Chỉ `/data` là vùng có thể bị ảnh hưởng.

---

## 2. Thay đổi tóm tắt

| Thành phần | Trước | Sau |
|---|---|---|
| Mount `/` | `rw` | `ro` |
| Bật cơ chế | — | `IMAGE_FEATURES += "read-only-rootfs"` |
| Kernel bootargs (U-Boot) | `root=... rw rootwait ...` | `root=... ro rootwait ...` |
| Vùng ghi persistent | rootfs còn rw nên ghi đâu cũng được | Chỉ có thể ghi vào `/data` |
| Vùng ghi tạm | rootfs | `/tmp`, `/var/volatile` (tmpfs) |
| SSH host key | sinh vào `/etc/ssh` | sinh vào `/var/run/ssh` (volatile) |

---

## 3. Chi tiết từng thay đổi

### 3.1. Bật feature

Trong file `meta-ota/recipes-core/images/core-image-home-gateway.bbappend`:

```diff
+ IMAGE_FEATURES:append = " read-only-rootfs"
```

Đặt ở meta-ota vì read only rootfs chỉ chạy đúng khi có partition `/data` làm vùng ghi và bootargs `ro` -> Gom cả cụm vào một layer cho mạch lạc.

Khi `read-only-rootfs` được bật trong `IMAGE_FEATURES` thì nó sẽ thực hiện `read_only_rootfs_hook` (nằm trong file `meta/classes/rootfs-postcommands.bbclass`) khi `do_rootfs`:
- Sửa `/etc/fstab`: đổi dòng của `/` từ "ghi được" sang **"chỉ đọc"**.
- Tạo `/etc/machine-id` rỗng: mỗi máy Linux cần một mã định danh riêng, bình thường được ghi lúc khởi động lần đầu. Nhưng giờ không ghi được, nên Yocto để sẵn một file rỗng và hệ thống sẽ cấp một mã tạm trong RAM mỗi lần boot.
- Xử lý khóa SSH: mỗi máy cần bộ khóa SSH riêng, bình thường được tạo và lưu vào `/etc/ssh`. Vì `/etc` đã khóa, Yocto chuyển khóa SSH được tạo trong RAM (`/var/run/ssh`). Hệ quả: mỗi lần khởi động lại, khóa SSH đổi mới -> máy tính khi kết nối SSH sẽ cảnh báo "khóa máy chủ đã thay đổi" (chấp nhận được với thiết bị phát triển; nếu muốn khóa cố định thì lưu sang `/data`).

### 3.2. Bootargs `rw` -> `ro`

Trong file `meta-ota/recipes-bsp/u-boot/files/0001-add-ota-boot-env.patch`:

```diff
-		"setenv bootargs root=/dev/mmcblk0p${mmc_part} rw rootwait console=ttyO0,115200n8 panic=10; " \
+		"setenv bootargs root=/dev/mmcblk0p${mmc_part} ro rootwait console=ttyO0,115200n8 panic=10; " \
```

`read_only_rootfs_hook` có thêm `APPEND:append = " ro"` để nối `ro` vào kernel cmdline. Nhưng trong dự án, kernel cmdline không lấy từ biến Yocto `APPEND` mà nó được `ota_boot` tự `setenv bootargs` trong built-in env của U-Boot. Nên `APPEND:append` không tới được cmdline thật -> phải đổi trực tiếp trong patch.

## 4. Risks có thể xảy ra

- **SSH host key volatile:** đổi mỗi lần boot, gây cảnh báo phía client. Nếu cần ổn định -> persist key sang `/data` (chưa làm trong thay đổi này).
- **Quên `/data` cho ghi mới:** code/feature sau này nếu ghi thẳng vào đường dẫn rootfs (vd `/etc`, `/usr`, `/var/lib` non-volatile) sẽ fail lúc runtime thay vì âm thầm ghi được như trước. Phải chủ động dùng `/data`. Đây là mặt tốt (lộ lỗi sớm) nhưng cần nhớ khi review.
- **Build QA:** nếu package nào cần ghi rootfs lúc cài/chạy mà không khai báo volatile, `do_rootfs` có thể cảnh báo — xử lý theo từng case khi gặp.
