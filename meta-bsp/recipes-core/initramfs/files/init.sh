#!/bin/sh

fatal() {
    echo "initramfs: FATAL: $1" > /dev/console
    sleep 2
    reboot -f
    # reboot -f không bao giờ return; vòng lặp dưới chỉ là chốt chặn
    while true; do sleep 1; done
}

mount -t proc proc /proc || fatal "mount /proc fail"
mount -t sysfs sysfs /sys || fatal "mount /sys fail"
mount -t devtmpfs devtmpfs /dev || fatal "mount /dev fail"

# root= trên cmdline do U-Boot set theo active slot (kernel có initramfs
# sẽ không tự mount root — root= ở đây chỉ là kênh truyền slot device)
ROOT_DEV=""
for arg in $(cat /proc/cmdline); do
    case "${arg}" in
        root=*) ROOT_DEV="${arg#root=}" ;;
    esac
done
[ -n "${ROOT_DEV}" ] || fatal "no root= on cmdline"

# Đợi MMC
retry=0
while [ ! -b "${ROOT_DEV}" ]; do
    retry=$((retry + 1))
    [ "${retry}" -le 10 ] || fatal "device ${ROOT_DEV} did not appear"
    sleep 1
done

[ -f /etc/verity.env ] || fatal "missing /etc/verity.env"
. /etc/verity.env
[ -n "${ROOT_HASH}" ] || fatal "ROOT_HASH is empty"
[ -n "${HASH_OFFSET}" ] || fatal "HASH_OFFSET is empty"

veritysetup open "${ROOT_DEV}" rootfs "${ROOT_DEV}" "${ROOT_HASH}" \
    --hash-offset="${HASH_OFFSET}" \
    || fatal "veritysetup open ${ROOT_DEV} failed (rootfs tampered?)"

mount -t ext4 -o ro /dev/mapper/rootfs /rootmnt || fatal "mount rootfs verity fail"

mount --move /dev /rootmnt/dev
umount /proc /sys

exec switch_root /rootmnt /sbin/init
fatal "switch_root fail"
