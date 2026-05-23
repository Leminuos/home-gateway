require conf/ota-version.inc

# OTA runtime packages.
IMAGE_INSTALL:append = " \
    swupdate             \
    swupdate-www         \
    libubootenv-bin      \
    ota-confirm-boot     \
"

WKS_FILE = "bbb-ota.wks"
IMAGE_FSTYPES = " wic wic.bmap ext4.gz"

do_image_wic[depends] += "virtual/bootloader:do_deploy"

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
