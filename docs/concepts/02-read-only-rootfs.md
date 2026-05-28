# Read only rootfs

---

# Phần 1: Nền tảng Linux Filesystem

## 1.1. Mount là gì, và file fstab dùng để làm gì

Trong Linux, mount là hành động gắn một filesystem vào một điểm trên cây thư mục. Không giống Windows có ổ `C:`, `D:` riêng biệt, Linux gắn mọi thứ vào cùng một cây. Khi ta mount một partition vào `/data`, mọi truy cập vào `/data` sẽ đọc/ghi trên partition đó.

Lệnh mount cơ bản:

```bash
mount /dev/mmcblk1p3 /data
```

Lệnh này nói với kernel: lấy filesystem trên partition thứ 3 của eMMC và gắn nó vào thư mục `/data`. Từ thời điểm này, ls `/data` sẽ hiện nội dung trên partition đó.

## 1.1.1. Mount options

Khi mount, ta có thể chỉ định các option ảnh hưởng đến cách filesystem hoạt động:

| Option | Ý nghĩa |
| --- | --- |
| `ro` — read-only | Không cho phép ghi. Đây chính là option cốt lõi khi nói đến read-only rootfs. Kernel mount rootfs với option này thì mọi lệnh ghi vào rootfs đều bị từ chối. |
| `rw` — read-write | Cho phép đọc/ghi bình thường. |
| `noatime` | Không cập nhật access time mỗi khi file được đọc. Trên embedded system, option này quan trọng vì giảm đáng kể số lần ghi vào storage, kéo dài tuổi thọ eMMC/SD card. |
| `nosuid` | Bỏ qua SUID/SGID bit trên filesystem đó, tăng bảo mật. |
| `nodev` | Không cho phép tạo device file, tăng bảo mật. |
| `noexec` | Không cho phép thực thi binary, tăng bảo mật. |

Ví dụ mount với nhiều option:

```bash
mount -t ext4 -o ro,noatime /dev/mmcblk1p2 /
```

## 1.1.2. Cách kernel mount rootfs

Khi hệ thống boot, bootloader truyền cho kernel tham số `root=` qua kernel command line, ví dụ:

```
root=/dev/mmcblk1p2 ro rootfstype=ext4
```

Tham số `ro` ở đây chính là chỗ quyết định rootfs được mount read-only. Kernel đọc tham số này, mount partition chỉ định vào `/`, rồi chạy init (systemd). Sau đó, init đọc `/etc/fstab` để mount các filesystem còn lại.

Với Yocto, khi ta bật `read-only-rootfs`, Yocto sẽ đảm bảo kernel command line có `ro` và fstab khai báo rootfs là read-only.

## 1.1.3. fstab

`/etc/fstab` là file khai báo các filesystem cần mount khi boot. Mỗi dòng gồm 6 trường:

```
# <device>         <mount point>  <type>   <options>              <dump> <pass>
/dev/mmcblk1p2     /              ext4     ro,noatime             0      1
/dev/mmcblk1p3     /data          ext4     rw,noatime             0      2
tmpfs              /tmp           tmpfs    defaults,size=32m      0      0
tmpfs              /var/run       tmpfs    defaults,size=5m       0      0
```

| Trường | Ý nghĩa |
| --- |---|
| device | Partition hoặc loại filesystem (tên device, UUID, hoặc tmpfs) |
| mount point | Thư mục gắn vào trên cây thư mục |
| type | Loại filesystem: ext4, tmpfs, vfat, none (cho bind mount) |
| options | Các mount option được phân cách bằng dấu phẩy |
| dump | Liên quan backup, thường để 0 trên embedded |
| pass | Thứ tự fsck kiểm tra: 1 cho rootfs, 2 cho partition khác, 0 bỏ qua |

## 1.1.4. Bind mount

Bind mount là một kỹ thuật đặc biệt — mount một thư mục (hoặc file) đã tồn tại lên một vị trí khác. Không phải mount device hay filesystem mới, mà là tạo một cửa sổ thứ hai cùng nhìn vào cùng nội dung.

```bash
mount --bind /data/etc/resolv.conf /etc/resolv.conf
```

Sau lệnh này, `/etc/resolv.conf` thực chất đang đọc/ghi vào `/data/etc/resolv.conf`. Đây là cách xử lý từng file cụ thể trong `/etc` mà không cần Overlay FS cho toàn bộ thư mục.

Bind mount cũng hoạt động với thư mục:

```bash
mount --bind /data/lib/NetworkManager /var/lib/NetworkManager
```

Trong fstab, bind mount được khai báo như sau:

```
/data/etc/resolv.conf  /etc/resolv.conf  none  bind  0  0
```

Bind mount là một kiểu mount đặc biệt rất quan trọng ở phần sau. Bình thường mount là gắn cả một vùng lưu trữ. Bind mount thì khác: nó làm một thư mục đã có sẵn hiện ra thêm ở một chỗ khác, như tạo một shortcut — nhưng cả hai chỗ là cùng một dữ liệu thật. Ghi ở chỗ này thì chỗ kia cũng thấy. Ta sẽ dùng cách này để lừa hệ thống ghi vào nơi cho phép (xem mục 5.2).

## 1.2. Các loại filesystem

**Nhóm 1 — Filesystem trên ổ đĩa/thẻ nhớ** (còn nguyên khi tắt nguồn):

- **ext4**: loại phổ biến nhất, ghi-đọc bình thường, có "nhật ký" (journal) giúp chống hỏng khi mất điện. Dự án dùng ext4 cho cả 3 phân vùng rootA / rootB / data.
- **squashfs**: loại **nén lại và chỉ cho đọc**. Ưu điểm: nhỏ hơn 50–70%. Vì chỉ đọc nên muốn ghi thì luôn phải ghép thêm một vùng ghi khác.

**Nhóm 2 — Filesystem trong RAM** (mất sạch khi tắt/khởi động lại):

- **tmpfs**: một ổ đĩa ảo nằm trong bộ nhớ RAM. Ghi rất nhanh, nhưng cúp điện hay reboot là sạch trơn. Đây là chỗ lý tưởng để chứa file tạm.

**Nhóm 3 — Virtual filesystem** (pseudo filesystem):

- **procfs** (`/proc`), **sysfs** (`/sys`), **devtmpfs** (`/dev`): không tốn dung lượng ổ đĩa nào cả, chỉ là cách nhân Linux export thông tin ra cho ta xem dưới dạng file.

---

# Phần 2: Read only rootfs là gì?

## 2.1. Rootfs là gì?

Rootfs hay root filesystem là filesystem được mount tại `/` — root của toàn bộ cây thư mục. Nó là filesystem đầu tiên mà kernel mount khi boot, và mọi filesystem khác đều được mount vào một thư mục con bên trong nó.

Trên BBB, rootfs thường nằm trên một partition của eMMC hoặc SD card. U-Boot truyền cho kernel biết rootfs ở đâu thông qua kernel command line:

```
root=/dev/mmcblk1p2 rootfstype=ext4
```

Kernel đọc tham số này, mount partition mmcblk1p2 vào `/` và bắt đầu chạy init.

Một điểm quan trọng cần hiểu: rootfs chứa gần như toàn bộ hệ điều hành — binary, library, config, script. Nó khác với boot partition (chỉ chứa kernel image, device tree, bootloader) và khác với data partition (chứa dữ liệu người dùng). Rootfs là "bộ xương" của hệ thống.

## 2.2. Read only rootfs là gì?

Read only rootfs đơn giản là rootfs được mount với option `ro` — kernel từ chối mọi thao tác ghi lên partition đó. Bất kỳ process nào cố tạo file, sửa file, hay xóa file trên rootfs đều nhận lỗi EROFS (Error Read-Only File System).

Điều này được thiết lập ở hai nơi:

**Thứ nhất,** trong kernel command line do bootloader truyền:

```
root=/dev/mmcblk1p2 ro rootfstype=ext4
```

Tham số `ro` ở đây khiến kernel mount rootfs ở chế độ read-only ngay từ đầu.

**Thứ hai,** trong `/etc/fstab`, rootfs được khai báo với option ro:

```
/dev/mmcblk1p2  /  ext4  ro,noatime  0  1
```

Điều này đảm bảo rằng nếu hệ thống cố remount rootfs, nó vẫn giữ chế độ read-only.

Khi rootfs là read-only, nội dung trên partition đó trở thành bất biến (immutable) — giống hệt lúc build image. Mỗi lần boot, hệ thống luôn nhìn thấy cùng một tập file, cùng nội dung, không có gì bị thay đổi.

## 2.3. Tại sao cần read only rootfs?

**Chống filesystem corruption**

Thiết bị có thể mất điện đột ngột bất cứ lúc nào, nếu rootfs đang được ghi dở vào thời điểm mất điện, filesystem có thể bị corrupt. Dù ext4 có journaling giúp recovery, nhưng journal cũng không đảm bảo 100% trong mọi trường hợp, và quá trình fsck sau crash tốn thời gian, làm chậm boot.

Với read only rootfs, không có thao tác ghi nào xảy ra trên partition đó, nên không thể bị corrupt. Thiết bị có thể mất điện ở bất kỳ thời điểm nào và rootfs vẫn được nguyên vẹn. Đây là yếu tố sống còn với các thiết bị hoạt động 24/7 trong môi trường gia đình, nơi mất điện là chuyện bình thường.

**Tuổi thọ storage**

eMMC và SD card có giới hạn về số lần ghi (write cycles). Mỗi cell trong flash memory chỉ chịu được một số lần xóa/ghi nhất định trước khi hỏng. Trên hệ thống read-write, rất nhiều thao tác ghi diễn ra liên tục mà ta không nhận ra, ví dự như logging, cập nhật atime khi đọc file, daemon ghi state, systemd ghi journal. Qua thời gian, những thao tác ghi liên tục này sẽ làm mài mòn storage.

**Bảo mật**

Khi rootfs là read only, attacker không thể thay đổi system binary, library, hay config. Không thể cài rootkit bằng cách thay thế `/usr/bin/ssh`, không thể chèn backdoor vào `/etc`, không thể modify init script để persist sau reboot. Đây là một lớp bảo mật cứng ở cấp filesystem mà phần mềm không thể bypass dễ dàng.

**Tính nhất quán**

Mỗi lần boot, hệ thống ở đúng trạng thái như lúc build image. Không có config bị sửa nhầm, không có file rác tích tụ, không có trạng thái lạ do ai đó SSH vào sửa thủ công rồi quên. Điều này rất có giá trị khi debug, ta biết chắc trạng thái hệ thống là gì vì nó không thể bị thay đổi. Nếu thiết bị hoạt động sai, ta chỉ cần kiểm tra image gốc và dữ liệu trên `/data`. Nó cũng giúp đảm bảo mọi thiết bị production chạy cùng một rootfs giống hệt nhau.

## 2.4. Bài toán cần giải quyết

Mặc dù có nhiều lợi ích nhưng read only rootfs cũng tạo ra một số vấn đề. Khi boot hệ thống lần đầu sẽ có hàng loại thứ cần ghi và nếu bật read only rootfs thì có thể sẽ fail. Một số vấn đề phổ biến mà ta sẽ gặp:
- Ghi log (nhật ký hoạt động)
- Tạo file đánh dấu tiến trình đang chạy (PID), socket, file khóa
- Lưu bộ nhớ đệm, dữ liệu làm việc của các dịch vụ
- Tạo mã định danh máy (`machine-id`), khóa SSH, danh sách máy chủ DNS
- Chương trình tạo file tạm

Toàn bộ phần 3 trở đi của mục lục sẽ tập trung vào cách giải quyết từng vấn đề này. Giải pháp tổng quát xoay quanh ba cơ chế chính: dùng tmpfs cho dữ liệu tạm mất được, dùng bind mount hoặc OverlayFS để chuyển hướng ghi sang nơi khác, và dùng partition persistent riêng cho dữ liệu cần giữ qua reboot. Kết hợp đúng ba cơ chế này, ta sẽ có một hệ thống vừa có rootfs bất biến vừa hoạt động bình thường.

---

# Phần 3: Các cơ chế cốt lõi

## 3.1. tmpfs

tmpfs là filesystem nằm hoàn toàn trong RAM. Nó không gắn với bất kỳ block device nào như eMMC hay SD card thay vào đó dữ liệu tồn tại trong bộ nhớ RAM, nên đọc/ghi cực nhanh, nhưng mất hết khi mất điện hoặc reboot.

Ta có thể hình dung đơn giản: **tmpfs biến một phần RAM thành ổ đĩa tạm thời.**

> [!WARNING]
> Các thư mục `/tmp`, `/run`, `/var/run` được mount tmpfs. Các thư mục này tồn tại trên hầu hết mọi hệ thống Linux bình thường, không liên quan gì đến read only rootfs.
>
> Đây là hành vi mặc định của linux và systemd. systemd luôn mount `/run` là tmpfs vì PID file, socket, lock file là dữ liệu runtime thuần túy. `/tmp` cũng tương tự — systemd có sẵn `tmp.mount` unit mount tmpfs lên `/tmp`.


### 3.1.1. Cách hoạt động bên trong

Linux kernel quản lý tmpfs thông qua cơ chế page cache — cùng cơ chế mà kernel dùng để cache file từ ổ đĩa thật. Điểm khác biệt là với file trên ổ đĩa, page cache là bản sao (mất cache thì đọc lại từ ổ đĩa), còn với temfs thì page cache chính là bản gốc duy nhất.

Cụ thể hơn:
- Khi ta ghi file vào tmpfs, kernel cấp các memory page (thường 4KB mỗi page) để chứa nội dung.
- Khi file bị xóa, page được giải phóng trả lại cho hệ thống.
- Các page này nằm trong RAM, được quản lý bởi Virtual Memory subsystem.
- Khi hệ thống chịu áp lực bộ nh, kernel có thể swap các page của tmpfs ra đĩa — đây là điểm khác so với ramfs (ramfs không bao giờ bị swap).
- tmpfs chỉ dùng RAM đúng bằng lượng dữ liệu thực sự được ghi vào, không phải toàn bộ size được cấp. Mount 100MB tmpfs nhưng chỉ ghi 2MB thì chỉ tốn 2MB RAM.

### 3.1.2. Cú pháp mount và các option

```bash
mount -t tmpfs tmpfs /mnt/ramdisk
```

Hoặc với các option:

```bash
mount -t tmpfs -o size=32m,mode=1777 tmpfs /tmp
```
Trong `/etc/fstab`:

```
tmpfs  /tmp       tmpfs  defaults,nosuid,nodev,size=32m,mode=1777  0  0
tmpfs  /var/run   tmpfs  defaults,nosuid,nodev,size=5m,mode=0755   0  0
```

Các option quan trọng:

| Option | Ý nghĩa |
| --- | --- |
| `size=` | Giới hạn dung lượng tối đa. Mặc định là 50% RAM nếu không chỉ định. |
| `mode=` | Permission khi mount.<br>- 1777 = sticky bit: ai cũng ghi được nhưng chỉ xóa được file của mình, giống `/tmp` chuẩn.<br>- 0755 cho thư mục chỉ root ghi được.
| `nosuid` | Không cho phép SUID bit — tăng bảo mật |
| `nodev` | Không cho phép device file — tăng bảo mật |
| `noexec` | Không cho phép execute binary — tăng bảo mật |
| `nr_inodes=` | Giới hạn số file/thư mục tối đa. Hữu ích để ngăn process nào đó tạo hàng triệu file nhỏ làm cạn kiệt inode. |

### 3.1.3. Cân nhắc trên BBB

BBB có 512MB RAM. Sau khi kernel và các service chiếm dụng, còn khoảng 300-400MB cho application. Ta cần cân đối giữa RAM cho tmpfs và RAM cho các process đang chạy.

Một phân bổ hợp lý cho home gateway:

```
/tmp        → 32MB   (file tạm của application)
/var/run    → 5MB    (PID files, socket — rất nhỏ)
/var/lock   → 2MB    (lock files — rất nhỏ)
/var/log    → 16MB   (log — nếu chọn lưu log trên RAM)
/var/tmp    → 16MB   (file tạm persist trong session)
```

Tổng khoảng 70MB, nhưng thực tế chỉ tốn vài MB vì hiếm khi dùng hết. Vẫn còn đủ RAM cho application. Ta kiểm tra dung lượng tmpfs đang dùng bằng cách:

```bash
df -h -t tmpfs
```

Nếu /var/log cần persistent (giữ log qua reboot để debug), đừng dùng tmpfs mà thay vào đó bind mount sang partition `/data`.

### 3.1.4. Điều gì xảy ra khi tmpfs đầy

Khi dữ liệu trong tmpfs chạm size limit, mọi thao tác ghi tiếp theo nhận lỗi ENOSPC (No space left on device) — giống hệt khi ổ đĩa thật đầy. Process ghi sẽ fail nhưng hệ thống không crash. Đây là lý do cần chọn size hợp lý và monitor dung lượng, đặc biệt với `/var/log` — nếu log quá nhiều có thể đầy tmpfs.

## 3.2. Bind mount

Như đã giới thiệu ở mục 1.1, bind mount làm một thư mục hiện ra ở chỗ khác, hai chỗ dùng chung một dữ liệu thật.

Bind mount là một cơ chế của Linux kernel cho phép ta lấy một thư mục hoặc file đã tồn tại ở vị trí A, rồi làm cho nó xuất hiện thêm ở vị trí B. Sau khi bind mount, cả hai đường dẫn A và B đều trỏ đến cùng một dữ liệu thật.

Khác với symlink - chỉ là con trỏ ở mức filesystem, bind mount hoạt động ở mức VFS trong kernel — nghĩa là nó transparent hoàn toàn, mọi process nhìn vào B đều thấy nội dung thật của A mà không biết đây là bind mount.

### 3.2.1. Cách hoạt động cơ bản

```bash
# Cú pháp
mount --bind <source> <target>

# Ví dụ đơn giản
mkdir /mnt/original
mkdir /mnt/mirror

echo "hello" > /mnt/original/test.txt

mount --bind /mnt/original /mnt/mirror

cat /mnt/mirror/test.txt    # → "hello"
echo "world" > /mnt/mirror/new.txt
ls /mnt/original/            # → test.txt  new.txt
```

Sau bind mount, `/mnt/mirror` và `/mnt/original` là hai cửa sổ cùng nhìn vào một dữ liệu. Ghi từ bên nào cũng thấy ở bên kia.

### 3.2.2. Symlink vs Bind mount

**Symlink**

Symlink là một file đặc biệt trên filesystem, trỏ từ đường dẫn A sang đường dẫn B.

Ví dụ minh họa:

```bash
ln -s /var/volatile/log /var/log
# /var/log -> /var/volatile/log
```

Nghĩa là khi process mở `/var/log/syslog`, kernel thấy symlink, tự động đi theo và mở `/var/volatile/log/syslog`.

Điểm quan trọng: symlink là một file nằm trên rootfs. Nó phải được tạo sẵn lúc build và nằm trong image. Trên hệ thống read only, ta không thể tạo thêm symlink lúc runtime.

Vì `/var/log` là symlink nên bản thân thư mục `/var/log` không thực sự tồn tại trên rootfs — nó chỉ là một con trỏ. Toàn bộ nội dung bên trong nằm trên tmpfs.

**Bind mount**

Bind mount hoạt động ở tầng khác. Nó nói với kernel: lấy nội dung từ thư mục A, đè lên thư mục B trong mount tree. Thư mục B vẫn tồn tại trên rootfs, nhưng khi process truy cập B, kernel trả về nội dung của A.

Ví dụ:


```bash
mount --bind /var/volatile/lib /var/lib
``` 

Thư mục `/var/lib` vẫn tồn tại trên rootfs read only với nội dung gốc, nhưng sau lệnh mount này mọi truy cập vào `/var/lib` sẽ thấy và ghi vào `/var/volatile/lib` trên tmpfs.

**Tại sao cần cả hai**

Symlink đơn giản hơn nhưng có giới hạn: Yocto phải quyết định lúc build rằng `/var/log` sẽ là symlink. Điều này nghĩa là trên rootfs không còn thư mục `/var/log` thật nữa, chỉ còn symlink. Yocto làm được điều này cho `/var/log`, `/var/run`, `/var/tmp` vì chúng là các đường dẫn chuẩn, biết trước, và không package nào cần thư mục thật ở đó.

Nhưng với `/var/lib` thì khác. Rất nhiều package lúc build cài file vào `/var/lib` — ví dụ systemd đặt file vào `/var/lib/systemd`, dbus đặt file vào `/var/lib/dbus`. Nếu Yocto biến `/var/lib` thành symlink trỏ sang `/var/volatile/lib`, toàn bộ nội dung gốc mà các package cài vào đó sẽ biến mất, vì /var/volatile là tmpfs trống rỗng mỗi lần boot.

Bind mount có thể giải quyết vấn đề này: `/var/lib` vẫn là thư mục thật trên rootfs chứa nội dung gốc do các package cài vào. Lúc runtime, bind mount đè lên nó bằng một thư mục writable trên tmpfs. Nội dung gốc bị che nhưng không bị mất dữ liệu gốc.

### 3.2.3. Cách kernel xử lý bind mount

Để hiểu sâu hơn, cần biết Linux VFS quản lý hai khái niệm riêng biệt: **dentry** (directory entry — đại diện cho tên file/thư mục trong filesystem) và **vfsmount** (đại diện cho một mount point).
Khi ta truy cập path `/var/lib/myapp/data.db`, kernel đi qua từng thành phần: `/` -> `var` -> `lib` -> `myapp` -> `data.db`. Tại mỗi bước, kernel kiểm tra mount table xem có mount point nào ở đây không.

Khi bind mount `/var/volatile/lib` lên `/var/lib`:

```
Trước bind mount:
    /var/lib -> dentry trên rootfs (ext4, read-only)

Sau bind mount:
    /var/lib -> vfsmount mới, trỏ đến dentry của
               /var/volatile/lib trên tmpfs (read-write)
```

Kernel thêm một entry vào mount table trong RAM. Rootfs trên đĩa không bị thay đổi gì. Khi process truy cập `/var/lib`, kernel thấy mount point, redirect sang tmpfs. Process không biết gì về chuyện này.

### 3.2.4. Ứng dụng cụ thể trong read only rootfs

**Bài toán**

Rootfs trên eMMC là read only. Nhưng `/var/lib` nằm trên rootfs đó, và nhiều service cần ghi vào `/var/lib`:

```
/var/lib/NetworkManager/  — lưu connection state
/var/lib/systemd/         — lưu random-seed, timers
/var/lib/dnsmasq/         — lưu DHCP leases
```

Nếu không xử lý gì, service ghi vào đây sẽ nhận lỗi:

```
Error: EROFS - Read-only file system
```

Giải pháp với bind mount

```
Boot sequence:

1. Kernel mount rootfs (read-only)
   /var/lib/ tồn tại nhưng read-only
   
2. systemd mount tmpfs lên /var/volatile (read-write, trong RAM)
   /var/volatile/ -> trống, writable

3. mkdir -p /var/volatile/lib
   Tạo thư mục trên tmpfs

4. mount --bind /var/volatile/lib /var/lib
   "Che" /var/lib read-only bằng /var/volatile/lib writable

5. Service start, ghi /var/lib/NetworkManager/...
   Thực chất ghi vào /var/volatile/lib/NetworkManager/... trên tmpfs
   -> Thành công, không lỗi
```

Rootfs trên đĩa vẫn nguyên vẹn. Dữ liệu runtime nằm trong RAM. Mất điện -> RAM mất -> boot lại sạch.

### 3.2.5. Bind mount cho persistent data

Không phải tất cả đều nên nằm trên tmpfs. Ví dụ DHCP lease database của dnsmasq — nếu mất sau mỗi reboot, tất cả client phải xin IP mới. Ta muốn giữ dữ liệu này khi reboot.

Giải pháp là bind mount từ persistent partition thay vì tmpfs:

```bash
# /data là partition read-write riêng trên eMMC
# Tạo sẵn thư mục
mkdir -p /data/lib/dnsmasq

# Bind mount
mount --bind /data/lib/dnsmasq /var/lib/dnsmasq
```

Lúc này dnsmasq ghi vào `/var/lib/dnsmasq`, thực chất ghi vào `/data/lib/dnsmasq` trên partition read-write. Rootfs vẫn read-only, dữ liệu vẫn persistent.

### 3.2.6. Bind mount cho single file

Bind mount không chỉ cho thư mục mà còn cho file đơn lẻ. Ví dụ kinh điển là `/etc/resolv.conf`:

```bash
# /etc nằm trên read-only rootfs
# Nhưng dhclient cần update /etc/resolv.conf khi nhận DNS mới

# Tạo file trên tmpfs
touch /run/resolv.conf

# Bind mount file
mount --bind /run/resolv.conf /etc/resolv.conf

# Bây giờ dhclient ghi /etc/resolv.conf → thực chất ghi /run/resolv.conf
```

Điều kiện: file target (`/etc/resolv.conf`) phải tồn tại sẵn trên rootfs lúc build. Nó có thể rỗng hoặc chứa nội dung mặc định — bind mount sẽ che nó lại. Đây là lý do trong Yocto recipe, ta thường thấy các file placeholder được tạo sẵn.

### 3.2.7. Bind mount trong systemd

Trên hệ thống dùng systemd (phổ biến trong Yocto hiện tại), bind mount được khai báo bằng `.mount ` unit thay vì script:

```ini
# var-lib-dnsmasq.mount
[Unit]
Description=Bind mount for dnsmasq data
DefaultDependencies=false
After=data.mount
Before=dnsmasq.service

[Mount]
What=/data/lib/dnsmasq
Where=/var/lib/dnsmasq
Type=none
Options=bind

[Install]
WantedBy=local-fs.target
```

Phần `Type=none` và `Options=bind` là cách systemd biểu diễn bind mount. `After=data.mount` đảm bảo partition `/data` đã mount xong trước khi bind mount.

## 3.3. OverlayFS

Hãy nghĩ về thư mục`/etc`, nó chứa hàng trăm file config và đa số là tĩnh, thiết lập lúc build, không bao giờ thay đổi. Nhưng có một số vài file cần thay đổi lúc runtime:

```
/etc/resolv.conf          — DNS, cập nhật bởi DHCP client
/etc/hostname             — có thể thay đổi qua web UI
/etc/hostapd/hostapd.conf — user thay đổi WiFi password
/etc/dnsmasq.conf         — thay đổi DHCP range
/etc/network/interfaces   — thay đổi network config
/etc/dropbear/            — SSH host key (tạo lần đầu boot)
```

Nếu dùng tmpfs cho toàn bộ `/etc`, ta mất hết config gốc. Nếu dùng bind mount, ta phải liệt kê từng file một — dễ bỏ sót và khó maintain.

-> OverlayFS giúp giải quyết bài toán này.

OverlayFS là filesystem cho phép xếp chồng nhiều layer lên nhau, tạo thành một view hợp nhất. Người dùng và application nhìn thấy một filesystem duy nhất, nhưng bên dưới thực chất có nhiều layer riêng biệt.

Hình dung đơn giản: ta có một tờ giấy in sẵn (không được viết lên), ta đặt một tờ giấy bóng kính trong suốt lên trên. Nhìn xuống thấy nội dung tờ giấy gốc. Khi cần sửa, ta viết lên tờ bóng kính — tờ gốc không bị ảnh hưởng. Muốn quay về bản gốc, bỏ tờ bóng kính đi.

### 3.3.1. Kiến trúc của OverlayFS

OverlayFS gồm bốn thành phần:

```
        +----------------------------------+
        |         merged (mount point)     |  <- Application nhìn thấy layer này
        |    Kết hợp upper + lower thành   |
        |         một view thống nhất      |
        +----------------------------------+
                   |
         +---------+----------+
         |                    |
   +------------+      +-------------+
   |   upper    |      |   lower     |
   |   (rw)     |      |   (ro)      |
   | Chứa mọi   |      | Dữ liệu     |
   | thay đổi   |      | gốc, không  |
   |            |      | bị sửa      |
   +────────────+      +─────────────+
         |
   +-------------+
   |  workdir   |
   | Kernel dùng|
   | nội bộ cho |
   | atomic ops |
   +-------------+
```

- **lowerdir:** layer read-only, chứa dữ liệu gốc. Trong trường hợp read only rootfs, đây chính là rootfs trên eMMC hay sdcard. OverlayFS không bao giờ ghi vào lower.
- **upperdir:** layer read-write, chứa mọi thay đổi so với lower. upperdir có thể nằm trên tmpfs hoặc trên partition persistent.
- **workdir:** thư mục làm việc nội bộ của kernel, phải nằm cùng filesystem với upper. Kernel dùng nó để đảm bảo các thao tác atomic (ví dụ rename file). Ta không cần quan tâm nội dung bên trong, chỉ cần tạo nó và để kernel quản lý.
- **merged:** mount point cuối cùng mà application nhìn thấy. Đây là kết quả hợp nhất của upper và lower.

### 3.3.2. Cú pháp mount

```bash
mount -t overlay overlay \
    -o lowerdir=/lower,upperdir=/upper,workdir=/work \
    /merged
```

Ví dụ cụ thể:

```bash
# Chuẩn bị
mkdir -p /mnt/lower /mnt/upper /mnt/work /mnt/merged

# Lower có sẵn dữ liệu
echo "original" > /mnt/lower/file1.txt
echo "keep me" > /mnt/lower/file2.txt

# Mount overlay
mount -t overlay overlay \
    -o lowerdir=/mnt/lower,upperdir=/mnt/upper,workdir=/mnt/work \
    /mnt/merged

# Kết quả
ls /mnt/merged/
# -> file1.txt  file2.txt   (thấy cả hai từ lower)

cat /mnt/merged/file1.txt
# -> "original"              (đọc từ lower)
```

### 3.3.3. Các thao tác cốt lõi

**Đọc file:** OverlayFS kiểm tra upperdir trước. Nếu file tồn tại trong upperdir, trả về file đó. Nếu không, trả về file từ lowerdir. Application không biết file đến từ layer nào, nó chỉ thấy một thư mục `/etc` bình thường với đầy đủ file.

```bash
cat /mnt/merged/file1.txt
# Kernel: file1.txt có ở upper không? -> Không
#         file1.txt có ở lower không? -> Có -> trả về nội dung từ lower
```

**Sửa file (copy-up):** OverlayFS thực hiện thao tác gọi là "copy-up" — copy file từ lowerdir lên upperdir, rồi apply thay đổi trên bản copy. Từ lần sau, file trong upperdir sẽ "che" file gốc trong lowerdir. File gốc trong lowerdir không bị động vào.

```
# file1.txt đang ở lower
echo "modified" > /mnt/merged/file1.txt

# Chuyện gì xảy ra bên trong:
# 1. Kernel copy /mnt/lower/file1.txt -> /mnt/upper/file1.txt
# 2. Kernel ghi "modified" vào /mnt/upper/file1.txt
# 3. /mnt/lower/file1.txt vẫn chứa "original" — không bị sửa

# Kiểm chứng
cat /mnt/lower/file1.txt     # -> "original"  (nguyên vẹn)
cat /mnt/upper/file1.txt     # -> "modified"  (bản copy đã sửa)
cat /mnt/merged/file1.txt    # -> "modified"  (upper ưu tiên hơn lower)
```

Copy-up chỉ xảy ra một lần, lần ghi đầu tiên. Các lần ghi sau vào file đó sẽ ghi trực tiếp vào bản trên upper vì nó đã tồn tại ở đó rồi.

**Xóa file:** OverlayFS tạo một "whiteout file" đặc biệt trong upperdir đánh dấu file đã bị xóa. File gốc vẫn còn trong lowerdir nhưng bị ẩn khỏi merged view.

```
rm /mnt/merged/file2.txt

# Chuyện gì xảy ra bên trong:
# 1. Kernel tạo whiteout file ở /mnt/upper/file2.txt
# 2. Khi liệt kê /mnt/merged/, kernel thấy whiteout -> ẩn file2.txt từ lower

ls /mnt/merged/
# -> file1.txt             (file2.txt biến mất)

ls /mnt/lower/
# -> file1.txt  file2.txt  (lower vẫn nguyên vẹn)
```

Nếu muốn phục hồi file đã xóa, chỉ cần xóa whiteout file ở upper:

```
bashrm /mnt/upper/file2.txt     # xóa whiteout
# file2.txt từ lower lại xuất hiện trong merged
```

**Tạo file mới:** File được tạo trực tiếp trong upperdir. lowerdir không bị ảnh hưởng.

### 3.3.4. upperdir trên tmpfs hay trên partition persistent

Đây là quyết định thiết kế quan trọng:

upperdir trên tmpfs: Mỗi lần boot, `/etc` trở về trạng thái gốc 100%.
- Ưu điểm là đơn giản, sạch, và đảm bảo tính nhất quán tuyệt đối.
- Nhược điểm: nếu user thay đổi config, thay đổi mất sau reboot — ta cần cơ chế riêng để persist những config này.

```bash
# upperdir trên tmpfs
mkdir -p /tmp/etc-upper /tmp/etc-work
mount -t overlay overlay \
  -o lowerdir=/etc,upperdir=/tmp/etc-upper,workdir=/tmp/etc-work \
  /etc
```

upperdir trên partition `/data`: thay đổi giữ sau khi reboot.
- Ưu điểm: Config mà user thay đổi nhự wiFi password, network settings,... vẫn còn sau khi reboot.
- Nhược điểm: hệ thống có thể "drift" khỏi trạng thái gốc theo thời gian, và nếu cần factory reset thì phải xóa upperdir.

### 3.3.5. Nhiều lowerdir

OverlayFS cho phép xếp nhiều lowerdir:

```bash
mount -t overlay overlay \
  -o lowerdir=/etc-custom:/etc-default,upperdir=...,workdir=... \
  /etc
```

lowerdir bên trái có độ ưu tiên cao hơn. File trong `/etc-custom` sẽ che file cùng tên trong `/etc-default`. Tính năng này hữu ích khi ta muốn có nhiều layer config (default -> product-specific -> runtime changes).

---

# Phần 4: Phân loại dữ liệu runtime

**Tại sao cần phân loại?**

Khi chạy hệ thống read only rootfs, mọi dữ liệu mà process tạo ra lúc runtime đều không thể ghi vào rootfs. Ta phải quyết định cho từng loại dữ liệu: nó sẽ nằm ở đâu, cơ chế nào xử lý, mất khi reboot có được không. Phân loại sai dẫn đến hoặc service fail, hoặc lãng phí tài nguyên, hoặc mất dữ liệu quan trọng.

Dữ liệu runtime được chia thành hai nhóm lớn dựa trên một tiêu chí duy nhất: có cần tồn tại sau khi reboot hay không.

## 4.1. Dữ liệu tạm (volatile)

Đây là dữ liệu chỉ có ý nghĩa trong lúc hệ thống đang chạy, mất khi reboot không gây hậu quả gì. Thực tế, mất đi còn tốt hơn vì đảm bảo hệ thống khởi động sạch.

**PID file**

Mỗi daemon khi start ghi process ID của mình vào file để init system và script khác biết PID mà quản lý. Ví dụ dnsmasq ghi `/var/run/dnsmasq.pid` chứa số 1234. Khi systemd cần stop dnsmasq, nó đọc file này để biết kill process nào. Sau reboot, process cũ không còn, PID cũ vô nghĩa, file mới sẽ được tạo khi service start lại.

**Socket file**

Các Unix domain socket mà process tạo để giao tiếp IPC. Ví dụ `/var/run/dbus/system_bus_socket` — mọi process cần nói chuyện với D-Bus đều connect qua socket file này. Sau reboot, dbus-daemon tạo lại socket mới.

**Lock file**

Dùng để đảm bảo chỉ một instance của service chạy tại một thời điểm, hoặc đồng bộ truy cập tài nguyên. Ví dụ `/var/lock/LCK..ttyS0` ngăn hai process cùng dùng serial port. Sau reboot, không process nào đang chạy nên lock không còn ý nghĩa.

**Cache tạm**

Dữ liệu cache mà application tạo ra để tăng tốc. Ví dụ: DNS cache, font cache, thumbnail cache...Mất cache thì application chỉ chậm hơn lần đầu rồi tự tạo lại.

**Giải pháp: tmpfs**

Tất cả dữ liệu trên đều thuộc về tmpfs. Mount tmpfs lên `/tmp`, `/var/run`, `/var/lock`, `/var/cache`. Dữ liệu tồn tại trong RAM lúc chạy, tự biến mất khi reboot, hệ thống luôn khởi động sạch.

```
tmpfs  /tmp       tmpfs  size=32m,mode=1777  0  0
tmpfs  /var/run   tmpfs  size=5m,mode=0755   0  0
tmpfs  /var/lock  tmpfs  size=2m,mode=1777   0  0
```

## 4.2. Dữ liệu persistent

Đây là dữ liệu mà người dùng hoặc hệ thống tạo ra lúc runtime và mất đi sẽ gây hậu quả — phải giữ qua reboot.

**Config đã thay đổi bởi user**

Network config: user thay đổi IP tĩnh, WiFi SSID/password, VLAN config qua web UI. Những thay đổi này nằm trong `/etc/network/`, `/etc/hostapd/`, `/etc/wpa_supplicant/`. Nếu mất sau reboot, user phải cấu hình lại từ đầu mỗi lần mất điện.

Service config: user tùy chỉnh DHCP range trong `/etc/dnsmasq.conf`, firewall rules trong `/etc/iptables/`, port forwarding, DNS server tùy chỉnh. Tất cả là config user đã sửa, phải giữ lại.

Certificates và keys: SSL cert cho web UI (`/etc/ssl/`), SSH host key (`/etc/dropbear/`, `/etc/ssh/`), VPN certificates. SSH host key đặc biệt quan trọng — nếu thay đổi mỗi boot, user sẽ nhận warning "host key changed" mỗi lần SSH vào.

**Database và state quan trọng**

Một số service duy trì database nhỏ cần persist:
- DHCP lease database: dnsmasq lưu danh sách thiết bị đã được cấp IP trong `/var/lib/misc/dnsmasq.leases`. Nếu mất file này sau reboot, dnsmasq vẫn hoạt động nhưng có thể cấp IP khác cho cùng thiết bị, gây phiền toái nếu user có port forwarding hoặc static mapping dựa trên lease cũ.
- MQTT persistent messages: nếu gateway chạy MQTT broker (mosquitto), các retained message và QoS 1/2 message đang chờ delivery cần persist để đảm bảo reliability.

**Log quan trọng**

Log nằm giữa ranh giới volatile và persistent, phụ thuộc vào yêu cầu của dự án.

Volatile logs: nếu gateway có kết nối cloud và đẩy log lên remote server real-time, log local không cần persist. Dùng tmpfs cho `/var/log`, log mất khi reboot không sao vì đã có bản trên cloud.

Persistent logs: nếu cần debug sau sự cố (tại sao gateway offline lúc 3 giờ sáng?), log phải persist qua reboot. Đặc biệt quan trọng trong giai đoạn development và field testing.

Hybrid: journald hỗ trợ cả hai: log vào `/run/log/journal` (volatile, tmpfs) hoặc `/var/log/journal` (persistent).

```
Nếu volatile:
    Đường dẫn: /var/log -> tmpfs
    Cơ chế: volatile-binds hoặc symlink đến /var/volatile/log

Nếu persistent:
    Đường dẫn: /var/log -> /data/log
    Cơ chế: bind mount từ /data

Lưu ý: Persistent log trên eMMC cần log rotation (logrotate)
       để tránh đầy partition. eMMC có giới hạn write cycle,
       log ghi liên tục sẽ giảm tuổi thọ.
```

**Giải pháp: partition persistent `/data`**

Tất cả dữ liệu persistent nằm trên một partition read-write riêng, mount tại `/data`:

```
/dev/mmcblk1p3  /data  ext4  rw,noatime,nosuid,nodev  0  2
```

## 4.4. Phương pháp phân tích cho từng service

Khi ta thêm một service mới, cần một quy trình có hệ thống để xác định service đó cần gì về mặt writable storage. Dưới đây là phương pháp thực hành.

### 4.4.1. Bước 1: Chuẩn bị môi trường phân tích

Ta cần một hệ thống BBB chạy image yocto chưa bật read only rootfs. Rootfs vẫn read-write bình thường để service hoạt động đúng hành vi mặc định. Ta sẽ quan sát hành vi rồi mới thiết kế giải pháp read-only.

Cài thêm công cụ phân tích vào image:

```bash
# Trong local.conf hoặc image recipe (chỉ dùng cho dev image)
IMAGE_INSTALL:append = " \
    inotify-tools \
    strace \
    lsof \
"
```

`inotify-tools` cung cấp `inotifywait` để monitor filesystem event real-time. `strace` theo dõi system call của process, bao gồm mọi thao tác file.

### 4.4.2. Bước 2: Thu thập dữ liệu

**Phương pháp 1: inotifywait — Theo dõi filesystem event**

Đây là phương pháp chính, cho cái nhìn tổng quan nhanh nhất.

```bash
# Theo dõi tất cả thay đổi trên các thư mục quan trọng
inotifywait -r -m \
    /etc /var /tmp /run /home \
    --format '%T %e %w%f' \
    --timefmt '%Y-%m-%d %H:%M:%S' \
    -e create -e modify -e delete -e moved_to -e moved_from \
    > /data/fs-events.log 2>&1 &

# Ghi lại PID để dừng sau
echo $! > /tmp/inotify.pid
```

Giải thích các flag:

`-r` theo dõi đệ quy toàn bộ cây thư mục. `-m` chạy liên tục (monitor mode), không thoát sau event đầu tiên. `--format` định dạng output cho dễ đọc. Các event: `create` (tạo file/thư mục mới), `modify` (ghi nội dung), `delete` (xóa), `moved_to`/`moved_from` (rename hoặc move).

Sau đó ta thao tác hệ thống: start/stop service, reboot, thay đổi config qua web UI, kết nối thiết bị IoT. Mọi thao tác ghi đều được ghi lại.

Output trông như thế này:

```
2024-03-15 10:00:01 CREATE /var/run/dnsmasq.pid
2024-03-15 10:00:01 MODIFY /var/run/dnsmasq.pid
2024-03-15 10:00:02 CREATE /var/lib/dnsmasq/dnsmasq.leases
2024-03-15 10:00:05 MODIFY /etc/resolv.conf
2024-03-15 10:01:00 MODIFY /var/lib/dnsmasq/dnsmasq.leases
2024-03-15 10:05:00 MODIFY /var/log/messages
```

**Phương pháp 2: strace — Theo dõi system call của từng process**

Khi cần phân tích sâu một service cụ thể, `strace` cho thấy chính xác từng thao tác file mà process thực hiện.

```bash
# Trace tất cả file-related syscall của dnsmasq
strace -f -e trace=open,openat,creat,rename,unlink,mkdir,write \
    -o /data/dnsmasq-trace.log \
    -p $(pidof dnsmasq)
```

`-f` theo dõi cả child process (quan trọng vì nhiều daemon fork). `-e trace=...` chỉ lọc các syscall liên quan đến file. `-p` attach vào process đang chạy.

Hoặc trace từ lúc service start:

```bash
# Thay đổi ExecStart tạm thời để trace

# Trong override unit
systemctl edit dnsmasq
# Thêm:
# [Service]
# ExecStart=
# ExecStart=/usr/bin/strace -f -e trace=file -o /data/dnsmasq-trace.log /usr/bin/dnsmasq -k
```

Output của `strace` rất chi tiết:

```
1234  openat(AT_FDCWD, "/var/run/dnsmasq.pid", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
1234  write(3, "1234\n", 5) = 5
1234  openat(AT_FDCWD, "/var/lib/dnsmasq/dnsmasq.leases", O_RDWR|O_CREAT, 0644) = 4
1234  openat(AT_FDCWD, "/etc/resolv.conf", O_RDONLY) = 5
```

Từ đây ta biết chính xác: dnsmasq mở `/var/run/dnsmasq.pid` để ghi (PID file -> volatile), mở `/var/lib/dnsmasq/dnsmasq.leases` để đọc-ghi (lease DB -> persistent), mở `/etc/resolv.conf` chỉ để đọc (không cần writable cho dnsmasq, nhưng dhclient thì cần ghi).

**Phương pháp 3: lsof — Snapshot file đang mở**

Khi service đang chạy ổn định, dùng `lsof` để xem nó đang giữ những file nào:

```bash
# Xem tất cả file mở bởi dnsmasq
lsof -c dnsmasq

# Output:
# COMMAND   PID USER  FD  TYPE DEVICE  SIZE  NODE NAME
# dnsmasq  1234 root  3w  REG  179,2   5     ...  /var/run/dnsmasq.pid
# dnsmasq  1234 root  4u  REG  179,2   1024  ...  /var/lib/dnsmasq/dnsmasq.leases
# dnsmasq  1234 root  5r  REG  179,2   45    ...  /etc/resolv.conf
# dnsmasq  1234 root  6u  IPv4 ...     0t0   UDP  *:53
```

Cột `FD` cho biết file descriptor và mode: `w` = write only, `r` = read only, `u` = read-write. Từ đây biết file nào service cần ghi.

Hoặc quét ngược — tìm tất cả process đang ghi vào thư mục cụ thể:

```bash
# Ai đang ghi vào /etc?
lsof +D /etc 2>/dev/null | grep -E 'w|u'

# Ai đang ghi vào /var?
lsof +D /var 2>/dev/null | grep -E 'w|u'
```

### 4.4.3. Bước 3: Phân loại và quyết định

Với mỗi file/thư mục cần writable mà ta phát hiện được, đặt câu hỏi: dữ liệu này mất khi reboot có sao không?
- Nếu không sao (PID file, socket, lock, cache) -> tmpfs.
- Nếu có vấn đề (config user thay đổi, database, credential) -> bind mount sang /data.
- Nếu không chắc -> bắt đầu với tmpfs. Nếu sau reboot hệ thống hoạt động bình thường mà không có dữ liệu đó, tmpfs là đủ. Nếu có vấn đề, chuyển sang persistent.

Ví dụ phân tích các service thường có trên home gateway:

**Ví dụ 1: dropbear (SSH server)**

```
Thu thập được:
    WRITE /var/run/dropbear.pid
    READ  /etc/dropbear/dropbear_rsa_host_key    — đọc nếu tồn tại
    WRITE /etc/dropbear/dropbear_rsa_host_key    — tạo nếu chưa có
    WRITE /etc/dropbear/dropbear_ecdsa_host_key  — tương tự
    READ  /etc/passwd, /etc/shadow               — authentication

Phân tích đặc biệt:
    Host key là trường hợp thú vị. Nó chỉ được tạo một lần (lần boot
    đầu tiên), sau đó chỉ đọc. Nhưng nó phải persistent — nếu thay đổi
    mỗi boot, SSH client sẽ báo "HOST KEY CHANGED" và từ chối kết nối.

Giải pháp:
    /var/run/dropbear.pid        -> tmpfs
    /etc/dropbear/               -> OverlayFS giữ host key persistent
    
    Hoặc tốt hơn: generate host key lúc build trong Yocto recipe,
    đưa vào rootfs sẵn. Lúc đó không cần ghi nữa.
```

**Ví dụ 2: mosquitto (MQTT broker)**

```
Thu thập được:
    WRITE /var/run/mosquitto.pid
    WRITE /var/lib/mosquitto/mosquitto.db        — retained messages DB
    WRITE /var/log/mosquitto/mosquitto.log       — log
    READ  /etc/mosquitto/mosquitto.conf

Phân loại:
┌──────────────────────────────────────┬────────────┬───────────────────────┐
│ File                                 │ Loại       │ Lý do                 │
├──────────────────────────────────────┼────────────┼───────────────────────┤
│ /var/run/mosquitto.pid               │ volatile   │ PID file              │
│ /var/lib/mosquitto/mosquitto.db      │ persistent │ Retained messages     │
│                                      │            │ cho IoT devices       │
│ /var/log/mosquitto/mosquitto.log     │ tùy chọn   │ Debug thì persistent  │
│ /etc/mosquitto/mosquitto.conf        │ persistent │ User config           │
└──────────────────────────────────────┴────────────┴───────────────────────┘

Giải pháp:
    /var/lib/mosquitto/  -> bind mount từ /data/lib/mosquitto/
```

---

# Phần 5: Read only rootfs trong Yocto

## 5.1. Bật feature

Trong Yocto, bật read only rootfs chỉ cần một dòng. Ta có thể đặt ở một trong các vị trí sau tùy phạm vi ảnh hưởng:

Trong `local.conf`: áp dụng cho tất cả image trong build hiện tại:

```bash
EXTRA_IMAGE_FEATURES += "read-only-rootfs"
```

Trong distro config: áp dụng cho toàn bộ distro:

```bash
EXTRA_IMAGE_FEATURES += "read-only-rootfs"
```

Trong image recipe: áp dụng cho riêng image đó:

```bash
IMAGE_FEATURES += "read-only-rootfs"
```

Khi ta bật feature này, Yocto sẽ thực hiện một chuỗi thay đổi trong quá trình build image:
- Sửa `/etc/fstab` để rootfs được mount với flag `ro`
- Yocto mount `var/volatile` thành tmpfs và tạo các symlink từ một số đường dẫn chuẩn trỏ vào đó. Những symlink này được tạo sẵn lúc build, nằm trong rootfs image. Cụ thể như sau:

   ```
   /var/volatile/      -> tmpfs
   /var/run            -> symlink đến /var/volatile/run
   /var/lock           -> symlink đến /var/volatile/lock
   /var/tmp            -> symlink đến /var/volatile/tmp
   /tmp                -> symlink đến /var/tmp (-> /var/volatile/tmp)
   ```

   Lúc boot, `/var/volatile` được mount tmpfs, script `populate-volatile.sh` chạy, đọc các file trong `/etc/default/volatiles/` và tạo cấu trúc thư mục cần thiết bên trong `/var/volatile` (tạo `/var/volatile/log`, `/var/volatile/run`... với đúng permission và owner).

- Biến `read-only-rootfs` được truyền vào quá trình build các recipe. Những recipe hỗ trợ read only rootfs sẽ điều chỉnh behavior.

  Ví dụ thay đổi đường dẫn ghi, tạo thêm symlink, hoặc tạo thư mục trên tmpfs thay vì trên rootfs. Không phải recipe nào cũng hỗ trợ, và đây là nguồn gốc của nhiều lỗi ta sẽ gặp.

  Nếu ta viết recipe riêng, ta cũng nên kiểm tra như sau:

  ```
  do_install:append () {
      if ${@bb.utils.contains('IMAGE_FEATURES', 'read-only-rootfs', 'true', 'false', d)}; then
          # Tạo volatile entry thay vì ghi trực tiếp
          echo "d root 0755 root root - /var/volatile/lib/myapp" \
              > ${D}${sysconfdir}/default/volatiles/99_myapp
      fi
  }
  ```

- Yocto có script kiểm tra rootfs sau khi build, cảnh báo nếu có file hay thư mục cần ghi mà chưa được xử lý. Tuy nhiên script này không phát hiện được tất cả trường hợp, nhiều vấn đề chỉ bộc lộ khi boot thật.

Sau khi build xong, ta có thể verify bằng cách mount rootfs image và kiểm tra fstab:

```bash
# Trong thư mục build
cat tmp/work/.../rootfs/etc/fstab
```

Hoặc boot lên BBB rồi kiểm tra:

```bash
mount | grep "on / "
```

Kết quả phải có "ro" trong mount options. Ví dụ: `/dev/mmcblk1p2 on / type ext4 (ro,noatime)`

## 5.2. volatile-binds

### 5.2.1. Vấn đề

Khi bật feature read only rootfs, mount `var/volatile` thành tmpfs và tạo các symlink như đã nói ở trên. Nhưng nhiều service cần ghi vào những đường dẫn nằm ngoài phạm vi trên.  Ví dụ NetworkManager ghi vào `/var/lib/NetworkManager`, systemd ghi vào `/var/lib/systemd`,... Đây là những đường dẫn nằm trên rootfs read only không có symlink sang `/var/volatile`, nên service sẽ fail.

Ngoài ra, ta không thể tạo symlink trên read only rootfs lúc runtime. Symlink phải tồn tại sẵn lúc build. Nếu ta quên một cái, service sẽ fail và ta phải rebuild cả image.

Đây chính là lý do `volatile-binds` tồn tại, nó giải quyết bằng bind mount:
- Tạo thư mục đích trên tmpfs (`/var/volatile/lib/` chẳng hạn)
- Copy nội dung gốc từ rootfs vào thư mục trên tmpfs (để giữ file/permission gốc)
- Bind mount thư mục trên tmpfs lên vị trí gốc (`/var/lib/`)

Sau bước 3, `/var/lib/` trỏ đến dữ liệu trên tmpfs -> ghi được bình thường, lại có sẵn nội dung ban đầu, nhưng mọi thay đổi nằm trong RAM sẽ mất sau khi reboot.

Cách thêm vào image:

```bash
IMAGE_INSTALL:append = " volatile-binds"
```

### 5.2.2. Source code thực tế

Ta đọc source code recipe `volatile-binds` để xem nó làm những gì:

```bash
find . -name "volatile-binds*" -path "*/meta/recipes*"
cat meta/recipes-core/volatile-binds/volatile-binds.bb
```

Tại đây, ta sẽ thêm nó có một biến `VOLATILE_BINDS`, đây chính là biến định nghĩa các bind mount cần tạo:

```bash
VOLATILE_BINDS ?= "\
    /var/volatile/lib /var/lib\n\
    /var/volatile/cache /var/cache\n\
    /var/volatile/spool /var/spool\n\
    /var/volatile/srv /srv\n\
"
```


Mỗi dòng gồm hai phần: source (trên tmpfs) và target (trên rootfs). Khi boot, target sẽ được bind mount từ source.

Ngoài ra, recipe có một hàm python đọc `VOLATILE_BINDS` và tạo ra các systemd mount unit tự động:

```bash
def volatile_systemd_services(d):
    services = []
    for line in oe.data.typed_value("VOLATILE_BINDS", d):
        if not line:
            continue
        what, where = line.split(None, 1)
        services.append("%s.service" % what[1:].replace("/", "-"))
    return " ".join(services)
```

Với entry `/var/volatile/lib /var/lib`, nó tạo ra hai file systemd:

**File 1: var-lib.mount — mount unit**

```ini
[Unit]
Description=Bind mount volatile /var/lib
DefaultDependencies=false
Before=local-fs.target

[Mount]
What=/var/volatile/lib
Where=/var/lib
Type=none
Options=bind
```

**File 2: var-lib.service — service unit đi kèm**

```ini
[Unit]
Description=Prepare volatile /var/lib
DefaultDependencies=false
Before=var-lib.mount
RequiresMountsFor=/var/volatile

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mkdir -p /var/volatile/lib
```

Nếu ta cần thêm bind mount cho service riêng, tạo bbappend cho volatile-binds:

```bash
# recipes-core/volatile-binds/volatile-binds.bbappend
```

### 5.2.3. Luồng hoạt động lúc boot

```
Kernel boot, mount rootfs (ro)
        │
        ▼
systemd start, mount /var/volatile (tmpfs, rw)
        │
        ▼
var-lib.service chạy:
    mkdir -p /var/volatile/lib
        │
        ▼
var-lib.mount chạy:
    mount --bind /var/volatile/lib /var/lib
        │
        ▼
Từ thời điểm này, mọi ghi vào /var/lib
thực chất ghi vào RAM (/var/volatile/lib)
        │
        ▼
Service khác start, ghi /var/lib bình thường
```

### 5.2.4. Thêm custom volatile bind

Ta có thể thêm entry riêng cho service của mình. Ví dụ hệ thống cần ghi vào `/var/lib/NetworkManager`:

**Cách 1: Append vào biến `VOLATILE_BINDS`**

Trong `local.conf` hoặc distro config:

```bash
VOLATILE_BINDS:append = "\
    /var/volatile/lib/NetworkManager /var/lib/NetworkManager\n\
"
```

**Cách 2: Trong bbappend của recipe**

Tạo file `volatile-binds_%.bbappend`:

```bash
VOLATILE_BINDS:append = "\
    /var/volatile/lib/NetworkManager /var/lib/NetworkManager\n\
"
```

**Cách 3: Tạo mount unit riêng trong recipe**

Nếu không muốn sửa `volatile-binds`, ta tự tạo systemd unit trong recipe:

**File `var-lib-NetworkManager.mount`:**

```ini
[Unit]
Description=Bind mount volatile NetworkManager state
DefaultDependencies=no
Before=NetworkManager.service
After=var-volatile.mount
RequiresMountsFor=/var/volatile
ConditionPathIsReadWrite=/var/volatile

[Mount]
What=/var/volatile/lib/NetworkManager
Where=/var/lib/NetworkManager
Type=none
Options=bind

[Install]
WantedBy=local-fs.target
```

Kèm theo một service để copy dữ liệu gốc trước khi bind mount:

```ini
[Unit]
Description=Populate volatile NetworkManager state
Before=var-lib-NetworkManager.mount
After=var-volatile.mount

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mkdir -p /var/volatile/lib/NetworkManager
ExecStart=/bin/cp -a /var/lib/NetworkManager/. /var/volatile/lib/NetworkManager/

[Install]
WantedBy=local-fs.target
```

### 5.2.5. volatile-binds vs persistent bind mount

Hiểu rõ sự khác biệt: `volatile-binds` bind mount từ tmpfs -> dữ liệu mất khi reboot. Nó phù hợp cho trạng thái mà service tạo lại khi khởi động.

Nếu ta cần dữ liệu giữ sau khi reboot, không dùng `volatile-binds` mà tạo bind mount từ partition `/data` — đây là phần ta phải làm thủ công vì Yocto không có cơ chế built-in cho persistent data partition.

## 5.3. systemd-tmpfiles

`systemd-tmpfiles` là cơ chế của systemd để tạo, dọn dẹp, và quản lý file/thư mục tạm khi boot. Nó đọc các rule file trong `/etc/tmpfiles.d/` và `/usr/lib/tmpfiles.d/`, rồi thực hiện: tạo thư mục, tạo file, set permission, tạo symlink, dọn file cũ.

Trên hệ thống read-only rootfs, systemd-tmpfiles đặc biệt quan trọng vì nhiều thư mục runtime cần tồn tại trên tmpfs trước khi service khởi động, nhưng tmpfs rỗng khi vừa mount.

Mỗi rule file có format:

```
# Type  Path                     Mode  User  Group  Age  Argument
d       /var/run/dnsmasq         0755  root  root   -    -
d       /var/log/mosquitto       0755  mosquitto mosquitto - -
f       /var/run/utmp            0664  root  utmp   -    -
L+      /var/lock                -     -     -      -    /var/volatile/lock
```

Các type phổ biến:

| Type | Ý nghĩa |
| --- | --- |
| `d` | tạo thư mục, set permission. Nếu thư mục đã tồn tại, chỉ set permission. |
| `f` | tạo file nếu chưa tồn tại. |
| `L+` | tạo symlink, xóa file cũ nếu cần. |
| `D`  | giống `d` nhưng xóa nội dung bên trong khi chạy `systemd-tmpfiles --clean`. |
| `w` | ghi nội dung vào file đã tồn tại. |

Ví dụ tạo file rule cho các service trên gateway:

```bash
# /etc/tmpfiles.d/bbb-gateway.conf

# Thư mục cho dnsmasq
d /var/run/dnsmasq      0755 root root -
d /var/lib/misc          0755 root root -

# Thư mục cho hostapd
d /var/run/hostapd       0755 root root -

# Thư mục cho mosquitto
d /var/log/mosquitto     0755 mosquitto mosquitto -
d /var/lib/mosquitto     0755 mosquitto mosquitto -

# Thư mục cho NetworkManager
d /var/lib/NetworkManager 0700 root root -
```

Ta tạo recipe hoặc bbappend để cài rule file vào image:

```bash
# recipes-core/bbb-gateway-tmpfiles/bbb-gateway-tmpfiles.bb

SUMMARY = "tmpfiles rules for BBB gateway"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=..."

SRC_URI = "file://bbb-gateway.conf"

do_install() {
    install -d ${D}${sysconfdir}/tmpfiles.d
    install -m 0644 ${WORKDIR}/bbb-gateway.conf ${D}${sysconfdir}/tmpfiles.d/
}
```

## 5.4. Xác nhận read only rootfs hoạt động đúng

Việc verify cần thực hiện ở ba giai đoạn:
- Kiểm tra hệ thống đã thực sự read-only chưa
- Tìm mọi lỗi ghi bị từ chối
- Stress test đảm bảo hệ thống ổn định.

Ta sẽ đi từ đơn giản đến nâng cao.

### 5.4.1. Giai đoạn 1: Xác nhận rootfs thực sự read-only

**Kiểm tra mount flags**

Việc đầu tiên là confirm rootfs đã mount với flag `ro`:

```bash
# Xem mount option của root filesystem
mount | grep "on / "

# Kết quả mong đợi:
# /dev/mmcblk0p1 on / type ext4 (ro,noatime)
#
# Nếu thấy "rw" -> read-only chưa hoạt động
```

Kiểm tra kỹ hơn qua `/proc/mounts`:

```bash
cat /proc/mounts | grep " / "

# Kết quả:
# /dev/root / ext4 ro,relatime 0 0
```

**Thử ghi trực tiếp**

Test thủ công đơn giản nhất — thử tạo file trên rootfs:

```bash
touch /test_readonly

# Kết quả mong đợi:
# touch: /test_readonly: Read-only file system

# Thử thêm vài vị trí
touch /usr/test
touch /bin/test
touch /lib/test

# Tất cả phải trả về "Read-only file system"
```

**Xác nhận các mount point writable đã đúng**

Ngược lại, các thư mục cần writable phải ghi được:

```bash
# tmpfs phải writable
touch /tmp/test && echo "OK: /tmp writable" && rm /tmp/test
touch /run/test && echo "OK: /run writable" && rm /run/test

# /data partition phải writable
touch /data/test && echo "OK: /data writable" && rm /data/test

# /etc overlay phải writable (nếu dùng overlayfs)
touch /etc/test_overlay && echo "OK: /etc overlay writable" && rm /etc/test_overlay
```

### 5.4.2. Giai đoạn 2: Phát hiện lỗi ghi bị từ chối

Đây là phần quan trọng nhất. Hệ thống có thể boot thành công nhưng nhiều service âm thầm fail khi cố ghi vào read only rootfs.

**Tìm lỗi EROFS trong system log**

Mỗi lần ghi vào read only filesystem, kernel trả về error code `EROFS`. Các service nhận lỗi này thường log ra:

```bash
# Tìm trong journal (systemd)
journalctl -b | grep -i "read-only"
journalctl -b | grep -i "EROFS"
journalctl -b | grep -i "Read-only file system"

# Tìm trong dmesg
dmesg | grep -i "read-only"

# Tìm rộng hơn — lỗi permission cũng có thể liên quan
journalctl -b | grep -iE "read-only|EROFS|permission denied|cannot create|cannot open|failed to write|failed to create|No such file"
```

**Kiểm tra service status**

Nhiều service fail im lặng hoặc chạy ở trạng thái degraded:

```bash
# Liệt kê tất cả service đang fail
systemctl --failed

# Kết quả ví dụ:
# UNIT                LOAD   ACTIVE SUB    DESCRIPTION
# dnsmasq.service     loaded failed failed DNS forwarder
# hostapd.service     loaded failed failed WiFi AP
# -> Cần điều tra từng cái

# Kiểm tra chi tiết service fail
systemctl status dnsmasq.service
journalctl -u dnsmasq.service -b

# Kiểm tra hệ thống có đang degraded không
systemctl is-system-running
# "running"  = tốt
# "degraded" = có service fail, cần kiểm tra
```

**Dùng inotifywait để monitor realtime**

Cách này không bắt được failed write vì `inotifywait` chỉ thấy event thành công, nhưng hữu ích để xem writable mount point nào đang được ghi, từ đó biết dữ liệu đã đi đúng chỗ chưa:

```bash
# Monitor mọi ghi vào /data
inotifywait -r -m /data --format '%T %w%f %e' \
    --timefmt '%Y-%m-%d %H:%M:%S' -e create -e modify -e delete

# Monitor mọi ghi vào /var/volatile (tmpfs)
inotifywait -r -m /var/volatile --format '%T %w%f %e' \
    --timefmt '%Y-%m-%d %H:%M:%S' -e create -e modify -e delete
```

**Dùng strace theo dõi system call**

Đây là cách mạnh nhất để phát hiện chính xác service nào cố ghi vào đâu. `strace` theo dõi mọi system call, ta lọc ra các call ghi file bị fail:

```bash
# Theo dõi một service cụ thể
# Tìm PID
pidof dnsmasq

# Attach strace
strace -p <PID> -e trace=open,openat,write,mkdir,rename,unlink \
    -f 2>&1 | grep "EROFS\|EACCES\|Read-only"

# Hoặc start service dưới strace
strace -f -e trace=open,openat,write,mkdir,rename,unlink \
    -o /tmp/dnsmasq_trace.log \
    /usr/bin/dnsmasq --no-daemon

# Sau đó tìm lỗi trong trace log
grep "EROFS\|EACCES\|Read-only" /tmp/dnsmasq_trace.log

# Kết quả ví dụ:
# openat(AT_FDCWD, "/var/lib/dnsmasq/dnsmasq.leases", O_WRONLY|O_CREAT, 0644) = -1 EROFS
# -> dnsmasq cố ghi lease file vào /var/lib/dnsmasq/ nhưng bị từ chối
# -> Cần bind mount /data/lib/dnsmasq -> /var/lib/dnsmasq
```

## 5.5. Khi gặp lỗi thì làm sao

### 5.5.1. Lỗi lúc build

**Package postinstall script fail.** Một số package có script chạy sau khi cài (postinst) cần ghi vào rootfs. Khi rootfs là read-only, script này fail lúc build hoặc bị defer sang first boot (rồi fail lúc boot). Triệu chứng: build warning hoặc boot log có lỗi postinst.

Cách xử lý: kiểm tra xem package có postinst script không:

```bash
# Trong thư mục build
cat tmp/work/<arch>/<package>/*/packages-split/<package>/postinst
```

Nếu postinst cần ghi file, ta phải tạo bbappend để sửa script hoặc đảm bảo file đích nằm trên writable area.

**`/etc/timestamp` không ghi được.** Yocto có cơ chế ghi timestamp vào `/etc/timestamp` lúc boot để xử lý hệ thống không có RTC. Trên read-only rootfs, thao tác này fail. Giải pháp: đảm bảo file nằm trên OverlayFS hoặc dùng `volatile-binds` cho file này.

### 5.5.2. Lỗi lúc boot

**"Read-only file system" trong journal.** Đây là lỗi phổ biến nhất. Ví dụ:

```
dnsmasq: failed to create /var/run/dnsmasq.pid: Read-only file system
```

Lỗi này cho biết chính xác file nào và service nào cần writable. Giải pháp: thêm tmpfs mount, bind mount, hoặc tmpfiles rule cho đường dẫn đó.

Quy trình debug:

```bash
# Xem tất cả lỗi từ lần boot hiện tại
journalctl -b --priority=err

# Xem service nào fail
systemctl --failed

# Xem chi tiết lỗi của một service
systemctl status dnsmasq.service
journalctl -u dnsmasq.service
```

**Service start quá sớm, trước khi tmpfs/bind mount sẵn sàng.** Service cần `/var/lib/something` writable, nhưng bind mount cho thư mục đó chưa hoàn thành khi service khởi động. Triệu chứng: service fail lần đầu nhưng restart thành công.

Giải pháp: thêm dependency trong systemd unit:

```ini
[Unit]
After=var-lib-something.mount
Requires=var-lib-something.mount
```

**/etc/machine-id rỗng hoặc không tồn tại.** systemd yêu cầu file này. Nếu rootfs có file rỗng, systemd cố ghi vào lần đầu boot -> fail trên read only rootfs. `volatile-binds` xử lý trường hợp này, nhưng ta cần đảm bảo `volatile-binds` được cài và chạy đúng.

Một cách tiếp cận khác: ghi `machine-id` lúc build bằng cách thêm vào image recipe:

```bash
ROOTFS_POSTPROCESS_COMMAND += "set_machine_id;"

set_machine_id() {
    # Tạo machine-id cố định hoặc để rỗng cho volatile-binds xử lý
    echo "uninitialized" > ${IMAGE_ROOTFS}/etc/machine-id
}
```

**D-Bus fail vì `/var/lib/dbus` không writable.** D-Bus lưu machine-id và state trong thư mục này. Giải pháp: `volatile-binds` hoặc bind mount.

### 5.5.3. Quy trình lặp

Triển khai read only rootfs trên thực tế là quy trình lặp:
1. Bật flag, build image
2. Flash lên BBB, boot
3. Đọc log lỗi
4. Xử lý lỗi đầu tiên — thêm tmpfs, bind mount, hoặc tmpfiles rule
5. Rebuild, reflash, boot lại
6. Kiểm tra lỗi đầu tiên đã hết, xử lý lỗi tiếp theo
7. Lặp cho đến khi không còn lỗi

Mỗi vòng lặp thường xử lý được một đến vài lỗi. Với nhiều hệ thống đầy đủ service, có thể cần 5-10 vòng lặp. Kiên nhẫn — mỗi lỗi đều có giải pháp rõ ràng khi ta đã hiểu ba cơ chế ở phần 3.

### 5.5.4. Mẹo giảm số vòng lặp

Thay vì build image mỗi lần sửa, ta có thể test nhanh trên BBB đang chạy:

```bash
# Remount rootfs read-write tạm thời để sửa
mount -o remount,rw /

# Sửa fstab, thêm tmpfiles rule, thêm mount unit...
# ...

# Remount read-only lại
mount -o remount,ro /

# Reboot để test
reboot
```

Sau khi xác định đủ các thay đổi cần thiết, đưa tất cả vào Yocto recipe một lần rồi build image chính thức.
