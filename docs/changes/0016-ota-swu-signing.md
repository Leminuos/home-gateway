# 0016 — OTA `.swu` signing

## Tóm tắt

Thêm cơ chế signature để verify `.swu` khi `secure-boot` bật.

## Lý do

Verified boot chỉ chặn kernel/FIT image không hợp lệ ở thời điểm boot; nó không tự đảm bảo package OTA tải từ host là package do build system tin cậy sinh ra. Signed `.swu` bổ sung lớp kiểm tra trước khi ghi vào slot inactive: nếu attacker thay `.swu`, sửa payload hoặc đưa package build bằng key khác, SWUpdate abort trước khi thay đổi A/B state.

## Chi tiết thay đổi

### `meta-ota/recipes-extended/images/beaglebone/sw-description.in`

```diff
                         filename           = "@IMAGE_NAME@";
                         device             = "/dev/mmcblk0p1";
                         type               = "raw";
                         compressed         = "zlib";
+                        sha256             = "$swupdate_get_sha256(@IMAGE_NAME@)";
                     }
@@
                         filename = "switch-slot.sh";
                         type     = "shellscript";
+                        sha256   = "$swupdate_get_sha256(switch-slot.sh)";
                     }
@@
                         filename           = "@IMAGE_NAME@";
                         device             = "/dev/mmcblk0p2";
                         type               = "raw";
                         compressed         = "zlib";
+                        sha256             = "$swupdate_get_sha256(@IMAGE_NAME@)";
                     }
@@
                         filename = "switch-slot.sh";
                         type     = "shellscript";
+                        sha256   = "$swupdate_get_sha256(switch-slot.sh)";
                     }
```

Khai báo hash cho rootfs image và postinstall script. Chữ ký CMS bảo vệ descriptor, còn SHA256 trong descriptor bảo vệ từng artifact nằm trong archive `.swu`.

### `meta-ota/recipes-extended/images/update-image.bb`

```diff
 SWUPDATE_IMAGES_FSTYPES[core-image-home-gateway] = ".ext4.gz"

+SWUPDATE_SIGNING = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'CMS', '', d)}"
+SWUPDATE_CMS_KEY  = "${TOPDIR}/keys/dev.key"
+SWUPDATE_CMS_CERT = "${TOPDIR}/keys/dev.crt"
+
 do_render_swdesc() {
```

`update-image` dùng `SWUPDATE_SIGNING = "CMS"` khi `secure-boot` có trong `DISTRO_FEATURES`. Key/cert được tái dùng từ cơ chế Verified Boot để tránh có hai nguồn trust riêng cho boot image và OTA package.

### `meta-ota/recipes-support/swupdate/files/signed-images.cfg`

```diff
+CONFIG_SIGNED_IMAGES=y
+CONFIG_SIGALG_CMS=y
+# CONFIG_SIGALG_RAWRSA is not set
+# CONFIG_SIGALG_RSAPSS is not set
+# CONFIG_SIGALG_GPG is not set
+CONFIG_CMS_IGNORE_CERTIFICATE_PURPOSE=y
+CONFIG_CMS_IGNORE_EXPIRED_CERTIFICATE=y
```

File config tách riêng phần signed-images để chỉ append vào SWUpdate defconfig khi `secure-boot` bật. `CONFIG_SIGALG_CMS` chọn verify CMS/X.509, đồng bộ với `SWUPDATE_SIGNING = "CMS"` ở recipe tạo `.swu`.

### `meta-ota/recipes-support/swupdate/files/swupdate.cfg.in`

```diff
     loglevel = 5;
     syslog   = true;
     reboot-required = false;
+@PUBLIC_KEY_LINE@
 };
```

Template runtime config có placeholder cho `public-key-file`. Khi secure mode bật, placeholder thành đường dẫn certificate; khi tắt, dòng placeholder bị xóa.

### `meta-ota/recipes-support/swupdate/swupdate_%.bbappend`

```diff
 SRC_URI:append = " \
     file://defconfig \
     file://swupdate.cfg.in \
     file://09-swupdate-args \
 "
 
+SRC_URI:append = " ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'file://signed-images.cfg', '', d)}"
+
 DEPENDS:append = " systemd"
 SYSTEMD_AUTO_ENABLE:${PN} = "disable"
 
+SWUPDATE_PUBKEY_DST = "${sysconfdir}/swupdate/swupdate.pem"
+SECURE_BOOT_ENABLED = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '1', '0', d)}"
+
+do_configure:prepend() {
+    if [ "${SECURE_BOOT_ENABLED}" = "1" ]; then
+        cat ${WORKDIR}/signed-images.cfg >> ${WORKDIR}/defconfig
+    fi
+}
+
+do_install[depends] += "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'virtual/kernel:do_kernel_generate_rsa_keys', '', d)}"
+
 do_install:append() {
@@
+    if [ "${SECURE_BOOT_ENABLED}" = "1" ]; then
+        install -d ${D}${sysconfdir}/swupdate
+        install -m 0644 ${TOPDIR}/keys/dev.crt ${D}${SWUPDATE_PUBKEY_DST}
+        sed -i "s#@PUBLIC_KEY_LINE@#    public-key-file = \"${SWUPDATE_PUBKEY_DST}\";#g" ${D}${sysconfdir}/swupdate.cfg
+    else
+        sed -i "/@PUBLIC_KEY_LINE@/d" ${D}${sysconfdir}/swupdate.cfg
+    fi
@@
 FILES:${PN}:append = " \
     ${sysconfdir}/swupdate.cfg \
     ${bindir}/09-swupdate-args \
+    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '${SWUPDATE_PUBKEY_DST}', '', d)} \
 "
```

SWUpdate target package giờ tự bật signed-images theo distro feature, cài certificate dùng để verify vào `/etc/swupdate/swupdate.pem`, render `public-key-file` trong `/etc/swupdate.cfg`, và khai báo dependency vào task sinh RSA keys của kernel để đảm bảo `${TOPDIR}/keys/dev.crt` tồn tại trước khi install package.

Đoạn `do_configure:prepend()` là phần build-time bắt buộc trong thiết kế hiện tại: `SRC_URI` chỉ đưa `signed-images.cfg` vào `${WORKDIR}`, còn SWUpdate chỉ bật verify signature nếu fragment đó được append vào `${WORKDIR}/defconfig` trước khi configure. `public-key-file` trong `swupdate.cfg` chỉ chọn certificate ở runtime; nó không tự bật `CONFIG_SIGNED_IMAGES` trong binary.

## Phân tích ảnh hưởng

Khi `secure-boot` bật, board chỉ cài `.swu` được ký bằng private key tương ứng với certificate đã được cài trong image đang chạy. Vì vậy khi đổi key production, cần rebuild và rollout image có certificate mới trước khi phát hành `.swu` ký bằng key mới, hoặc thiết kế cơ chế chuyển key riêng.

Nếu `.swu` bị sửa sau khi build, payload copy lỗi, manifest trỏ tới file cũ, hoặc host server phục vụ nhầm package build bằng key khác, SWUpdate sẽ fail ở bước verify và không chạy `switch-slot.sh`; `active_slot`, `ustate`, `boot_count` không nên thay đổi.

Dev mode có thể tắt `secure-boot` để build/cài package không ký, nhưng mode đó cũng tắt Verified Boot theo thiết kế hiện tại. Không dùng dev mode cho image production.

Signed `.swu` không thay thế dm-verity: sau khi hệ thống đã boot, rootfs vẫn chưa có cơ chế chống sửa block device runtime. Lớp bảo vệ này chỉ xác minh OTA package trước khi cài.
