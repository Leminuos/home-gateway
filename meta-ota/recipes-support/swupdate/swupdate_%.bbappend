FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

require conf/ota-version.inc

SRC_URI:append = " \
    file://defconfig \
    file://swupdate.cfg.in \
    file://09-swupdate-args \
"

DEPENDS:append = " systemd"
SYSTEMD_AUTO_ENABLE:${PN} = "disable"

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/swupdate.cfg.in ${D}${sysconfdir}/swupdate.cfg
    sed -i "s#@BOARD_NAME@#${OTA_BOARD_NAME}#g" ${D}${sysconfdir}/swupdate.cfg

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
"
