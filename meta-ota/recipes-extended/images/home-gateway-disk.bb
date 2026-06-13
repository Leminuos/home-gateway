LICENSE = "MIT"

require conf/ota-version.inc

COMPATIBLE_MACHINE = "bbb-home-gateway"

PACKAGE_INSTALL = ""
IMAGE_FSTYPES = "wic wic.bmap"
IMAGE_FEATURES = ""
IMAGE_LINGUAS = ""

WKS_FILE = "bbb-ota.wks.in"

DEPENDS += "dosfstools-native mtools-native"

inherit image

INITRAMFS_IMAGE = ""
INITRAMFS_IMAGE_BUNDLE = "0"

SECURE_BOOT = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '1', '0', d)}"

ROOTFS_ARTIFACT = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', \
    'core-image-home-gateway-${MACHINE}.ext4.verity', \
    'core-image-home-gateway-${MACHINE}.ext4', d)}"

BOOT_PART_SIZE_KiB = "24576"

do_bootfs[cleandirs] = "${WORKDIR}/bootfs"
do_bootfs() {
    bootdir="${WORKDIR}/bootfs"
    vfat="${WORKDIR}/boot-home-gateway.vfat"

    if [ "${SECURE_BOOT}" = "1" ]; then
        fit=$(ls -1 ${DEPLOY_DIR_IMAGE}/fitImage-${INITRAMFS_IMAGE_BOOT}-* 2>/dev/null | head -n1)
        [ -n "${fit}" ] || bbfatal "initramfs fitImage not found in ${DEPLOY_DIR_IMAGE}"
        install -m 0644 "${fit}" ${bootdir}/fitImage
    else
        install -m 0644 ${DEPLOY_DIR_IMAGE}/zImage ${bootdir}/zImage
        install -m 0644 ${DEPLOY_DIR_IMAGE}/am335x-boneblack.dtb ${bootdir}/am335x-boneblack.dtb
    fi

    rm -f "${vfat}"
    mkfs.vfat -n bootA -C "${vfat}" ${BOOT_PART_SIZE_KiB}
    mcopy -i "${vfat}" -s ${bootdir}/* ::/

    install -m 0644 "${vfat}" ${DEPLOY_DIR_IMAGE}/boot-home-gateway.vfat
    gzip -9 -c "${vfat}" > ${DEPLOY_DIR_IMAGE}/boot-home-gateway.vfat.gz
}
addtask bootfs before do_image_wic after do_rootfs

INITRAMFS_IMAGE_BOOT = "home-gateway-initramfs"

do_bootfs[depends] += "virtual/kernel:do_deploy"
do_image_wic[depends] += "virtual/bootloader:do_deploy data-partition:do_deploy core-image-home-gateway:do_image_complete"
