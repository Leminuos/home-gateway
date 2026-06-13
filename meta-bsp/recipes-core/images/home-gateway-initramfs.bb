LICENSE = "MIT"

PACKAGE_INSTALL = "busybox cryptsetup initramfs-verity-boot"

NO_RECOMMENDATIONS = "1"

IMAGE_FSTYPES = "${INITRAMFS_FSTYPES}"
IMAGE_FEATURES = ""
IMAGE_LINGUAS = ""
IMAGE_ROOTFS_SIZE = "8192"
IMAGE_ROOTFS_EXTRA_SPACE = "0"

inherit image

IMAGE_CLASSES:remove = "image_types_wic"

COMPATIBLE_MACHINE = "bbb-home-gateway"

VERITY_IMAGE ?= "core-image-home-gateway"
do_rootfs[depends] += "${VERITY_IMAGE}:do_image_complete"

bake_verity_env() {
    install -d ${IMAGE_ROOTFS}${sysconfdir}
    install -m 0644 ${DEPLOY_DIR_IMAGE}/${VERITY_IMAGE}-${MACHINE}.ext4.verity.env \
        ${IMAGE_ROOTFS}${sysconfdir}/verity.env
}
ROOTFS_POSTPROCESS_COMMAND += "bake_verity_env; "

python () {
    if 'secure-boot' not in (d.getVar('DISTRO_FEATURES') or '').split():
        raise bb.parse.SkipRecipe("home-gateway-initramfs is only used when the secure-boot distro feature is enabled")
}
