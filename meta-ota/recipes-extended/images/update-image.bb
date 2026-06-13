LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

require conf/ota-version.inc

PV = "${OTA_SW_VERSION}"

inherit swupdate

COMPATIBLE_MACHINE = "bbb-home-gateway"

FILESEXTRAPATHS:prepend := "${THISDIR}:"
SRC_URI = " \
    file://beaglebone/sw-description.in \
    file://beaglebone/switch-slot.sh \
"

IMAGE_NAME = "${IMAGE_BASENAME}-${MACHINE}-${PV}"
IMAGE_LINK_NAME = "${IMAGE_BASENAME}-${MACHINE}"

IMAGE_DEPENDS = "core-image-home-gateway home-gateway-disk"

SWUPDATE_IMAGES = "boot-home-gateway core-image-home-gateway"

ROOT_FSTYPE = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '.ext4.verity.gz', '.ext4.gz', d)}"
SWUPDATE_IMAGES_FSTYPES[core-image-home-gateway] = "${ROOT_FSTYPE}"
SWUPDATE_IMAGES_FSTYPES[boot-home-gateway] = ".vfat.gz"
SWUPDATE_IMAGES_NOAPPEND_MACHINE[boot-home-gateway] = "1"

do_render_swdesc() {
    boot_image="boot-home-gateway.vfat.gz"
    root_image="core-image-home-gateway-${MACHINE}${ROOT_FSTYPE}"

    sed -e "s|@VERSION@|${OTA_SW_VERSION}|g" \
        -e "s|@HW_REVISION@|${OTA_HW_REVISION}|g" \
        -e "s|@BOARD_NAME@|${OTA_BOARD_NAME}|g" \
        -e "s|@BOOT_IMAGE@|${boot_image}|g" \
        -e "s|@ROOT_IMAGE@|${root_image}|g" \
        ${WORKDIR}/beaglebone/sw-description.in > ${WORKDIR}/sw-description

    install -m 0755 ${WORKDIR}/beaglebone/switch-slot.sh ${WORKDIR}/switch-slot.sh
}

addtask render_swdesc after do_unpack before do_swuimage
