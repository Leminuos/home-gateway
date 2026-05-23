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

# images to build before building swupdate image
IMAGE_DEPENDS = "core-image-home-gateway"

# images and files that will be included in the .swu image
SWUPDATE_IMAGES = "core-image-home-gateway"

SWUPDATE_IMAGES_FSTYPES[core-image-home-gateway] = ".ext4.gz"

do_render_swdesc() {
    image_name="core-image-home-gateway-${MACHINE}.ext4.gz"

    sed -e "s|@VERSION@|${OTA_SW_VERSION}|g" \
        -e "s|@HW_REVISION@|${OTA_HW_REVISION}|g" \
        -e "s|@BOARD_NAME@|${OTA_BOARD_NAME}|g" \
        -e "s|@IMAGE_NAME@|${image_name}|g" \
        ${WORKDIR}/beaglebone/sw-description.in > ${WORKDIR}/sw-description

    install -m 0755 ${WORKDIR}/beaglebone/switch-slot.sh ${WORKDIR}/switch-slot.sh
}

addtask render_swdesc after do_unpack before do_swuimage