FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

require conf/ota-version.inc

SRC_URI:append = " \
    file://defconfig \
    file://swupdate.cfg.in \
    file://09-swupdate-args \
"

SRC_URI:append = " ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'file://signed-images.cfg', '', d)}"

DEPENDS:append = " systemd"
SYSTEMD_AUTO_ENABLE:${PN} = "disable"

SWUPDATE_PUBKEY_DST = "${sysconfdir}/swupdate/swupdate.pem"
SECURE_BOOT_ENABLED = "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '1', '0', d)}"

do_configure:prepend() {
    if [ "${SECURE_BOOT_ENABLED}" = "1" ]; then
        cat ${WORKDIR}/signed-images.cfg >> ${WORKDIR}/defconfig
    fi
}

do_install[depends] += "${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'virtual/kernel:do_kernel_generate_rsa_keys', '', d)}"

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/swupdate.cfg.in ${D}${sysconfdir}/swupdate.cfg
    sed -i "s#@BOARD_NAME@#${OTA_BOARD_NAME}#g" ${D}${sysconfdir}/swupdate.cfg

    if [ "${SECURE_BOOT_ENABLED}" = "1" ]; then
        install -d ${D}${sysconfdir}/swupdate
        install -m 0644 ${TOPDIR}/keys/dev.crt ${D}${SWUPDATE_PUBKEY_DST}
        sed -i "s#@PUBLIC_KEY_LINE@#    public-key-file = \"${SWUPDATE_PUBKEY_DST}\";#g" ${D}${sysconfdir}/swupdate.cfg
    else
        sed -i "/@PUBLIC_KEY_LINE@/d" ${D}${sysconfdir}/swupdate.cfg
    fi

    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/09-swupdate-args ${D}${bindir}/09-swupdate-args
    sed -i \
        -e "s#@BOARD_NAME@#${OTA_BOARD_NAME}#g" \
        -e "s#@HW_REVISION@#${OTA_HW_REVISION}#g" \
        ${D}${bindir}/09-swupdate-args
}

FILES:${PN}:append = " \
    ${sysconfdir}/swupdate.cfg \
    ${bindir}/09-swupdate-args \
    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', '${SWUPDATE_PUBKEY_DST}', '', d)} \
"
