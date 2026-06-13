# 0012 — dm-verity rootfs

## Tóm tắt

Bổ sung dm-verity cho rootfs vào tính năng secure boot (trước đây chỉ có U-Boot Verified Boot). Mỗi block của rootfs được xác minh khi đọc dựa trên một hash tree; root hash của hash tree được đưa vào initramfs, mà initramfs lại nằm trong FIT image đã ký RSA2048 — nên root hash thừa hưởng nguyên secure chain của U-Boot Verified Boot. Để fitImage không nằm trong chính vùng dữ liệu mà nó bảo vệ-> gây ra vấn đề circular hash, layout disk sẽ được mở rộng từ 3 -> 5 partition. fitImage giờ sẽ được chuyển sang boot partition riêng với mỗi slot. Layout 5 partition dùng chung cho cả khi `secure-boot` tắt (boot partition khi đó chứa `zImage` + dtb, rootfs là ext4 thường, không verity).

## Lý do

U-Boot Verified Boot chỉ xác minh kernel/dtb lúc boot, rootfs vẫn sửa được trực tiếp trên block device sau khi mount. Kẻ tấn công có quyền ghi vào thẻ MMC có thể chỉnh sửa rootfs mà không bị phát hiện. dm-verity giúp loại bỏ lỗ hổng này: mọi block rootfs đọc ra đều phải khớp hash tree, sai một byte -> đọc lỗi -> reboot -> rollback A/B.

Việc đặt root hash ở đâu là vấn đề quan trong nhất. Ba phương án được đưa ra:
- Phương án 1: Lưu vào U-Boot env -> env raw không được ký -> tự vô hiệu hóa mục tiêu chống tamper
- Phương án 2: Lưu vào fitImage và đưa fitImage vào rootfs -> gây ra vấn đề circular hash (xem mục ngay dưới)
- Phương án 3: Lưu vào fitImage và đưa fitImage vào boot partition (mỗi slot sẽ có boot partition riêng)

Chốt phương án 3, cụ thể lý do lựa chọn và traceoff giữa các phương án được nói trong docs `docs/decisions/02-dm-verity-partition.md`

## Vấn đề circular hash

Điều mà ta mong muốn là root hash của rootfs được ký để không sửa được -> nên cần đưa nó vào FIT image, vì nó đã được sign và được verify từ uboot. Nhưng nếu vẫn để fitImage nằm trong rootfs thì sẽ gặp vòng lặp như sau:

```
root hash = hash(toàn bộ rootfs)
                     │
   rootfs chứa /boot/fitImage  ──►  fitImage chứa initramfs  ──►  initramfs chứa root hash
        ▲                                                                      │
        └──────────────────────────────────────────────────────────────────────┘
```

- Để tính root hash thì cần phải có nội dung cuối cùng của rootfs mà rootfs lại cần chứa fitImage.
- Để generate fitImage thì lại cần phải có root hash để ghi vào initramfs.

Hai việc phụ thuộc vòng tròn, không cái nào làm trước được. Về bản chất là tìm nội dung `X` sao cho `hash(X) = h` trong khi `h` được nhúng bên trong `X` — bất khả thi.

**Giải pháp:** đưa fitImage ra ngoài rootfs. Khi fitImage nằm ở một boot partition riêng, `hash(rootfs)` không còn phụ thuộc vào fitImage -> tính được root hash trước -> đưa vào initramfs -> build và ký fitImage sau.

Tuy nhiên, điều này có nghìa là ta cần hai parition khác nhau cho mỗi slot -> làm tăng số lượng partition.

**Lưu ý — có hai vòng lặp khác nhau, đừng nhầm:**

- **circular hash**: mang tính *toán học*, không thể phá bằng cách sắp xếp lại build. Bắt buộc fitImage ra khỏi rootfs.
- **circular dependency bulld (bitbake task graph)** (rootfs -> root hash -> initramfs -> fitImage -> rootfs): phá được bằng cách tách việc lắp đĩa sang recipe `home-gateway-disk` và gỡ kernel khỏi rootfs.

## Chi tiết thay đổi

### meta-bsp/classes/verity-image.bbclass

**Mục đích:** biến rootfs ext4 thành verity image để kernel có thể kiểm tra tính toàn vẹn từng block khi đọc.

**dm-verity cần 3 thứ:**

1. **data:** chính là rootfs ext4.
2. **Hash tree:** với mỗi block 4096 byte của data, tính một hash SHA256; rồi gom các hash đó thành hash block và tiếp tục thực hiện hash, cứ thế lên tới một hash duy nhất ở đỉnh. Đây là một khối dữ liệu riêng.
3. **Root hash:** hash ở đỉnh cây. Chỉ cần giữ đúng một chuỗi 64 ký tự hex này là xác minh được toàn bộ rootfs: đọc block nào, kernel băm lên theo cây, tới đỉnh phải khớp root hash, sai một byte là phát hiện ngay.

**Conversion type trong Yocto là gì:** khi `IMAGE_FSTYPES` liệt kê. Ví dụ `ext4.gz`, Yocto sẽ build ext4 trước rồi chạy một lệnh conversion để biến nó thành `.gz`. Class này đăng ký thêm một conversion type mới tên `verity`, để khi liệt kê `ext4.verity` thì Yocto tự chạy bước tạo hash tree:

```bitbake
CONVERSIONTYPES += "verity"
CONVERSION_DEPENDS_verity = "cryptsetup-native"   # cần veritysetup lúc build
CONVERSION_CMD:verity = "verity_image_create ${IMAGE_NAME}${IMAGE_NAME_SUFFIX}.${type} ${IMAGE_LINK_NAME}.${type}.verity.env"
```

**Hàm `verity_image_create` làm gì?**

Nó chạy với cwd là thư mục deploy, nhận hai tham số:
- `$1`: tên file ext4
- `$2`: tên symlink cho file `.env`:

1. Copy `...ext4` -> `...ext4.verity`
2. Padding file cho tròn bội số 4096 bằng `truncate -s %4096`.
3. Chạy `veritysetup format --hash-offset=<kích thước data> ...ext4.verity ...ext4.verity`: tính hash tree và append vào ngay sau vùng data trong cùng một file. Kết quả: một file duy nhất có layout `[ ext4 data ][ hash tree ]`. `--hash-offset` chính là offset nơi hash tree bắt đầu.
4. `veritysetup` in ra dòng `Root hash:` -> dùng `awk '/^Root hash:/ { print $NF }'` lấy field cuối của dòng đó -> ghi file vào `...ext4.verity.env` gồm `ROOT_HASH=...` và `HASH_OFFSET=...`.
5. Tạo symlink (`...ext4.verity.env` theo `IMAGE_LINK_NAME`) để recipe khác tham chiếu được mà không cần biết version trong tên file.

**Vì sao gộp data + hash vào một file:** để bước sau ghi rawcopy vào một partition là xong; lúc boot, initramfs mở lại bằng `veritysetup open ... --hash-offset=<offset>` để kernel biết phần nào là data, phần nào là hash.

**File `.env` là mắt xích quan trọng:** nó mang `ROOT_HASH` ra ngoài để đưa vào initramfs.

### meta-bsp/recipes-kernel/linux/files/dm-verity.cfg  + linux-yocto_%.bbappend

**Mục đích:** bật các tính năng kernel cần để hiểu và dùng được thiết bị dm-verity. Mặc định kernel linux-yocto cho BBB không bật sẵn, nên phải thêm một config fragment khi build với `secure-boot`.

Từng option và lý do:

| Option | Để làm gì |
|---|---|
| `CONFIG_MD` | Bật device-mapper. Là nền tảng cho mọi target dm-*. |
| `CONFIG_BLK_DEV_DM` | Cho phép tạo thiết bị device-mapper (`/dev/mapper/...`). |
| `CONFIG_DM_VERITY` | Chính là target verity: kiểm tra hash từng block khi đọc. |
| `CONFIG_CRYPTO_SHA256` | Thuật toán hash verity. |
| `CONFIG_BLK_DEV_LOOP` | Dự phòng cho thao tác trên file image; cũng hữu ích khi debug. |

**Cách apply fragment** — chỉ thêm khi `secure-boot` bật:

```bitbake
SRC_URI += "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'file://dm-verity.cfg', '', d)}"
KERNEL_CONFIG_FRAGMENTS:append = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', ' ${WORKDIR}/dm-verity.cfg', '', d)}"
```

- `SRC_URI` mang file `.cfg` vào thư mục build;
- `KERNEL_CONFIG_FRAGMENTS` bảo kernel-yocto merge nó vào `.config` cuối cùng.

### meta-bsp/recipes-core/initramfs/

Hai file: một script `/init` và một recipe đóng gói nó.

**`files/init.sh`:** script chạy đầu tiên khi kernel nạp xong initramfs. Nó thực hiện:
1. Mount `/proc`, `/sys`, `/dev`.
2. Đọc `root=` từ `/proc/cmdline`. U-Boot set giá trị này theo slot đang boot (vd `/dev/mmcblk0p2` cho slot A) — initramfs dùng nó để biết partition rootfs nào cần mở.
3. Đợi device MMC xuất hiện.
4. `source /etc/verity.env` -> lấy `ROOT_HASH` và `HASH_OFFSET`
5. `veritysetup open ${ROOT_DEV} rootfs ${ROOT_DEV} ${ROOT_HASH} --hash-offset=${HASH_OFFSET}` -> tạo device `/dev/mapper/rootfs`. Từ đây mỗi lần đọc block sẽ đều được verify.
6. `mount -t ext4 -o ro /dev/mapper/rootfs /rootmnt` -> mount rootfs thật
7. `switch_root /rootmnt /sbin/init` → trao quyền cho rootfs thật, initramfs biến mất khỏi RAM.

**Quan trọng:** bất kỳ bước nào fail (root hash sai do rootfs bị sửa, mount lỗi...) đều gọi `reboot -f`. Reboot khiến U-Boot tăng `boot_count`; quá ngưỡng -> rollback sang slot kia. Nhờ vậy rootfs hỏng/bị tamper tự động dẫn tới rollback thay vì treo.

**`initramfs-verity-boot.bb`:** recipe cài `init.sh` thành `/init` trong initramfs, khai báo `RDEPENDS = "busybox cryptsetup"` để chắc chắn có `sh` và `veritysetup` trong initramfs.

### meta-bsp/recipes-core/images/home-gateway-initramfs.bb

Initramfs là một hệ thống file nhỏ được kernel nạp thẳng vào RAM và chạy trước khi mount rootfs thật. Nhiệm vụ duy nhất của nó là mở khoá dm-verity cho rootfs.

**Vì sao cần initramfs**

Kernel không thể mount thẳng `/dev/mmcblk0p2` làm root được vì rootfs đó là dm-verity. Phải có ai đó chạy veritysetup open để tạo ra `/dev/mapper/rootfs` (thiết bị ảo có kiểm tra hash từng block) rồi mới mount -> chính là initramfs này.

Và quan trọng hơn, đây là lý do thiết kế root hash nằm trong fitImage. Initramfs được đóng vào FIT image và ký RSA cùng kernel. Vì `/etc/verity.env` (chứa root hash) nằm trong initramfs, nên root hash cũng nằm trong chuỗi tin cậy đã ký.

Và recipe này có nhiệm vụ đóng gói initramfs đó thành một file `cpio.gz` để nhét vào FIT. Nội dung tối thiểu:

```bitbake
PACKAGE_INSTALL = "busybox cryptsetup initramfs-verity-boot"
```

- `busybox`: cung cấp `sh`, `mount`, `switch_root`, `reboot`...
- `cryptsetup`: cung cấp `veritysetup`.
- `initramfs-verity-boot`: package cài script `/init`.

Chặn các package được `cryptsetup` kéo theo như `kernel-module` để initramfs nhỏ gọn:

```bitbake
NO_RECOMMENDATIONS = "1"
```

Định dạng output lấy theo `INITRAMFS_FSTYPES` (thường là `cpio.gz`) và bỏ hết feature/language thừa:

```bitbake
IMAGE_FSTYPES = "${INITRAMFS_FSTYPES}"
IMAGE_FEATURES = ""
IMAGE_LINGUAS = ""
IMAGE_ROOTFS_SIZE = "8192"
IMAGE_ROOTFS_EXTRA_SPACE = "0"
```

Build như một image, bỏ `image_types_wic` vì initramfs không cần tạo wic image:

```bitbake
iherit image
IMAGE_CLASSES:remove = "image_types_wic"
```

**Mấu chốt — nhét root hash vào initramfs:** initramfs phải biết root hash của rootfs, mà root hash chỉ có sau khi `core-image-home-gateway` build xong và chạy conversion verity -> Đoạn code sau ép rootfs build trước và initramfs build sau:

```bitbake
VERITY_IMAGE ?= "core-image-home-gateway"
do_rootfs[depends] += "${VERITY_IMAGE}:do_image_complete"
```

Hàm `bake_verity_env` copy file `.env` mà `verity-image.bbclass` vừa sinh ra vào `/etc/verity.env` của initramfs:

```bitbake
bake_verity_env() {
    install -d ${IMAGE_ROOTFS}${sysconfdir}
    install -m 0644 ${DEPLOY_DIR_IMAGE}/${VERITY_IMAGE}-${MACHINE}.ext4.verity.env \
        ${IMAGE_ROOTFS}${sysconfdir}/verity.env
}
ROOTFS_POSTPROCESS_COMMAND += "bake_verity_env; "
```

Khi `secure-boot` tắt thì không cần verity, không cần initramfs —> recipe tự loại khỏi build:


```bitbake
python () {
    if 'secure-boot' not in (d.getVar('DISTRO_FEATURES') or '').split():
        raise bb.parse.SkipRecipe("home-gateway-initramfs is only used when the secure-boot distro feature is enabled")
}
```

### meta-bsp/conf/machine/bbb-home-gateway.conf

```bitbake
INITRAMFS_IMAGE         = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'home-gateway-initramfs', '', d)}"
INITRAMFS_IMAGE_BUNDLE  = "0"
MACHINE_ESSENTIAL_EXTRA_RDEPENDS:remove = "kernel-image kernel-devicetree"
```

Giải thích từng dòng:

- `INITRAMFS_IMAGE` — chỉ định image initramfs nào sẽ được đóng vào FIT. Đặt `home-gateway-initramfs` khi `secure-boot` bật, để trống khi tắt (mode tắt không cần initramfs).
- `INITRAMFS_IMAGE_BUNDLE = "0"` — bảo `kernel-fitimage.bbclass` đóng initramfs thành **một node `ramdisk` riêng** trong FIT (thay vì nhồi chung vào kernel image). Quan trọng: ở chế độ này, class tự đưa `"ramdisk"` vào danh sách `sign-images`, tức **initramfs cũng được ký RSA cùng kernel + fdt** — đúng điều ta cần để root hash (nằm trong initramfs) được bảo vệ.
- `MACHINE_ESSENTIAL_EXTRA_RDEPENDS:remove = "kernel-image kernel-devicetree"` — gỡ kernel và dtb khỏi rootfs (cả 2 mode), vì giờ chúng nằm ở boot partition. Hệ quả phụ rất quan trọng: rootfs không còn phụ thuộc gói kernel → **phá được vòng lặp** rootfs → root hash → initramfs → fitImage → (nếu vẫn ở trong rootfs) → rootfs.

### meta-bsp/recipes-bsp/u-boot — 0001-bbb-ota.cfg

Hai thay đổi layout buộc U-Boot phải biết thêm 2 thứ (áp cho cả 2 mode):

- `CONFIG_FS_FAT` + `CONFIG_CMD_FAT` — boot partition giờ là FAT, nên U-Boot cần lệnh `fatload` để đọc fitImage/zImage từ đó. (Trước đây kernel nằm trong rootfs ext4 nên dùng `ext4load`.)
- `CONFIG_EFI_PARTITION` — bảng phân vùng đổi từ MBR sang **GPT**; nếu không bật, U-Boot không đọc/đánh số được các partition GPT → không tìm thấy boot/root partition.

### meta-ota/recipes-core/images/core-image-home-gateway.bbappend

Image này giờ chỉ sinh rootfs, không còn tự lắp `.wic`. Các điểm chính:

- **Bỏ wic, đổi `IMAGE_FSTYPES`:** khi `secure-boot` bật -> `inherit verity-image` và `IMAGE_FSTYPES = "ext4 ext4.verity ext4.verity.gz"`. Tức build ext4 thường -> tạo bản verity (`.ext4.verity`, dùng để ghi vào partition) -> nén gz (`.ext4.verity.gz`, dùng cho OTA). Mode tắt thì chỉ `ext4` + `ext4.gz`.
- **Cố định kích thước rootfs:** `IMAGE_ROOTFS_SIZE = "147456"` + `IMAGE_OVERHEAD_FACTOR = "1.0"`. Vì sao cần cố định: ảnh rootfs được ghi rawcopy vào partition nên phải có trần chắc chắn và verity tính hash theo kích thước data cố định, nếu kích thước nhảy lung tung thì offset/hash khó kiểm soát. Rootfs vượt 144MiB -> build fail có chủ ý.
- **Vẫn build 1 lệnh:** `do_build[depends] += "home-gateway-disk:do_image_complete"` — gõ `bitbake core-image-home-gateway` sẽ tự kéo recipe lắp đĩa chạy sau cùng.

### meta-ota/recipes-extended/images/home-gateway-disk.bb

Trước đây `core-image-home-gateway` vừa build rootfs vừa tạo wic. Nhưng với dm-verity thì nó nảy sinh vấn đề circular dependency build:

```
core-image-home-gateway:do_image_complete
   └─ core-image-home-gateway:do_image_wic    (wic cần fitImage)
        └─ kernel:do_deploy                   (fitImage cần initramfs)
             └─ initramfs:do_image_complete
                  └─ core-image-home-gateway:do_image_complete   ◄── quay lại chính nó
```

-> Nếu để chung việc generate rootfs và disk image trong một recipe thì bitbake không giải được thứ tự.

Giải pháp ở đây là tách đôi vai trò:
- `core-image-home-gateway` sẽ chỉ có nhiệm vụ là generate rootfs (`.ext4` / `.ext4.verity`), không generate disk image.
- Một recipe khác sẽ chuyển generate disk image, nó được chạy sau cùng khi mọi artifact đã build xong rồi mới tiền hành assembly.

-> Nhờ vậy đồ thị phụ thuộc thành đường thẳng, không vòng.

**Giải thích từng phần**

Recipe này không có rootfs riêng, mục tiêu duy nhất của nó là tạo `wic` và `wic.bmap`:

```bitbake
PACKAGE_INSTALL = ""
IMAGE_FSTYPES = "wic wic.bmap"
WKS_FILE = "bbb-ota.wks.in"
```

Recipe không nhúng initramfs do initramfs đã nằm sẵn trong fitImage. Đặt rỗng để tránh kéo theo logic initramfs vào đây:

```bitbake
INITRAMFS_IMAGE = ""
INITRAMFS_IMAGE_BUNDLE = "0"
```

Cần `mkfs.vfat` và `mcopy` để tạo partition boot dạng FAT trong task d`o_bootfs`:

```bitbake
DEPENDS += "dosfstools-native mtools-native"
```

Recipe này không có rootfs riêng (`PACKAGE_INSTALL = ""`), nó chỉ đi gom artifact của recipe khác trong `DEPLOY_DIR_IMAGE` rồi xếp vào partition.

Chọn file rootfs theo mode:
- `secure-boot` bật: dùng verity image (`.ext4.verity`)
- `secure-boot` tắt: dùng ext4 image thường

```bitbake
ROOTFS_ARTIFACT = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', \
    'core-image-home-gateway-${MACHINE}.ext4.verity', \
    'core-image-home-gateway-${MACHINE}.ext4', d)}"
```

**Task `do_bootfs`:** Đóng gói nội dung boot partition:

Tuỳ mode mà nó gom nội dung khác nhau:
- `secure-boot` bật -> copy `fitImage` vào FAT.
- `secure-boot` tắt -> copy `zImage` + `am335x-boneblack.dtb` vào FAT.

Sau đó đóng thành FAT image:

```bitbake
mkfs.vfat -n bootA -C "${vfat}" ${BOOT_PART_SIZE_KiB}    # tạo FAT 24MB rỗng
mcopy -i "${vfat}" -s ${bootdir}/* ::/                   # nhét nội dung bootfs vào
install ... ${DEPLOY_DIR_IMAGE}/boot-home-gateway.vfat   # deploy
gzip ... > ${DEPLOY_DIR_IMAGE}/boot-home-gateway.vfat.gz # bản nén cho OTA
```

**Task `do_image_wic`:** chạy file wks để ráp đĩa cuối cùng, đổ từng artifact vào đúng partition.

**Vẫn build 1 lệnh:** `core-image-home-gateway` khai báo `do_build[depends] += "home-gateway-disk:do_image_complete"`, nên gõ `bitbake core-image-home-gateway` sẽ tự kéo recipe lắp đĩa chạy theo. 3 vùng raw đầu của wks (MLO/u-boot/env) giữ nguyên offset như layout cũ → tương thích `fw_env.config` và quy trình flash bootloader.

### `wic/bbb-ota.wks.in`

**Vì sao wks dùng `--source rawcopy` chứ không phải `--source rootfs`**:

- `--source rootfs` bảo wic dựng một ext4 mới từ thư mục rootfs của recipe đang chạy wic. Nhưng recipe này có rootfs rỗng -> partition sẽ rỗng. Hơn nữa ext4 dựng mới khác từng byte so với ext4 đã tính root hash, và **không có hash tree** đính kèm → dm-verity fail ngay.
- `--source rawcopy` đổ **nguyên xi** một file đã build sẵn vào partition. Đây là điều bắt buộc: partition rootfs phải chứa đúng từng byte file `…ext4.verity` mà root hash (đã ký trong FIT) trỏ tới. Chỉ rawcopy mới giữ nguyên được.

### meta-ota/recipes-bsp/u-boot/files — 2 patch env

Logic boot A/B nằm trong built-in env của U-Boot. Layout 5 partition buộc sửa 3 chỗ trong cả hai patch:

- **`ota_pick_slot`:** trước đây mỗi slot chỉ có 1 partition rootfs. Giờ mỗi slot có 2 partition nên đặt hai biến:
  - slot A -> `boot_part=1`, `root_part=2`
  - slot B -> `boot_part=3`, `root_part=4`.
- **`ext4load` -> `fatload`:** kernel/fitImage giờ nằm ở boot partition FAT.
- **bootargs:** `root=/dev/mmcblk0p${root_part}`: chỉ định partition rootfs theo slot.

### meta-ota/recipes-extended/images/update-image.bb + beaglebone/sw-description.in

Đây là phần đóng gói file `.swu`. Vì mỗi slot giờ gồm 2 partition, gói OTA phải ghi 2 image cho mỗi slot thay vì 1:

- `sw-description.in`: install set:
  + `copy1` ghi boot.vfat -> bootA(p1) và rootfs -> rootA(p2)
  + `copy2` ghi -> bootB(p3) và rootB(p4).
  SWUpdate chọn copy nào tùy slot đang chạy.
- `SWUPDATE_IMAGES` thêm artifact `boot-home-gateway` (`.vfat.gz`) bên cạnh rootfs, để cả hai cùng được nhét vào `.swu`.
- Payload rootfs đổi theo mode: `.ext4.verity.gz` (secure bật) / `.ext4.gz` (tắt).

## Phân tích ảnh hưởng

- **Layout đổi 3 → 5 GPT là breaking change về đĩa**: thẻ SD đang chạy layout cũ **không OTA lên được** (sw-description ghi vào p3/p4 chưa tồn tại) → phải reflash toàn bộ `.wic` một lần. Sau đó OTA A/B hoạt động bình thường.
- **rootfs mất `/boot`**: kernel/dtb/fitImage không còn trong rootfs. Mọi script/tài liệu giả định `/boot/fitImage` phải cập nhật.
- **Chuỗi tin cậy**: U-Boot pubkey (trong u-boot.dtb) → verify FIT → initramfs (chứa root hash) → veritysetup → từng block rootfs. Sửa 1 byte rootfs → đọc lỗi → reboot → rollback. Sửa fitImage → U-Boot từ chối boot → rollback.
- **Dung lượng**: rootfs cố định 144MiB + hash tree ~1% < 160MB partition; fitImage (kernel + initramfs ~4-5MB + dtb) ~10-11MB < 24MB boot partition. Nếu rootfs vượt 144MiB → build fail (chủ ý).
- **GPT backup header** nằm ở cuối image; khi flash `.wic` lên thẻ lớn hơn image, kernel sẽ cảnh báo "primary GPT ... backup at end" (vô hại, sửa được bằng `sgdisk -e`).
- **Mode `secure-boot` tắt vẫn dùng layout 5 partition**: boot partition chứa zImage + dtb, rootfs ext4 thường, U-Boot `bootz`. Đảm bảo đường build cũ không vỡ, chỉ khác số partition.
