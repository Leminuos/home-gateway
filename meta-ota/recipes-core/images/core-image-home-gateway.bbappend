require conf/ota-version.inc

# OTA runtime packages.
IMAGE_INSTALL:append = " \
    swupdate             \
    swupdate-www         \
    libubootenv-bin      \
    ota-confirm-boot     \
    data-partition       \
"

inherit ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'verity-image', '', d)}
IMAGE_FSTYPES = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'ext4 ext4.verity ext4.verity.gz', 'ext4 ext4.gz', d)}"

IMAGE_ROOTFS_SIZE = "147456"
IMAGE_OVERHEAD_FACTOR = "1.0"
IMAGE_ROOTFS_EXTRA_SPACE = "0"

EXTRA_IMAGECMD:ext4 += "-b 4096"

# Rootfs mount read-only
IMAGE_FEATURES:append = " read-only-rootfs"

do_build[depends] += "home-gateway-disk:do_image_complete"

# /etc/hwrevision — sw-description dùng để check hardware compatibility
# /etc/sw-version — SWUpdate dùng để so sánh version khi có rule no-downgrade
write_ota_metadata() {
    install -d ${IMAGE_ROOTFS}${sysconfdir}
    echo "${OTA_BOARD_NAME} ${OTA_HW_REVISION}" > ${IMAGE_ROOTFS}${sysconfdir}/hwrevision
    echo "${OTA_SW_VERSION}"         > ${IMAGE_ROOTFS}${sysconfdir}/sw-versions
}

# Mountpoint cho phân vùng /data
create_data_mountpoint() {
    install -d -m 0755 ${IMAGE_ROOTFS}/data
}

ROOTFS_POSTPROCESS_COMMAND += "write_ota_metadata; create_data_mountpoint; "
