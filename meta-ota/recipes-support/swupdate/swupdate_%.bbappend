FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

require conf/ota-version.inc

SRC_URI:append = " \
    file://defconfig \
    file://swupdate.cfg.in \
    file://09-swupdate-args \
"

DEPENDS:append = " systemd"

do_install:append() {
    install -d ${D}${libdir}/swupdate/conf.d
    install -m 0644 ${WORKDIR}/09-swupdate-args ${D}${libdir}/swupdate/conf.d/09-swupdate-args
    sed -i \
        -e "s#@MACHINE@#${MACHINE}#g" \
        -e "s#@BOARD_NAME@#${OTA_BOARD_NAME}#g" \
        -e "s#@HW_REVISION@#${OTA_HW_REVISION}#g" \
        ${D}${libdir}/swupdate/conf.d/09-swupdate-args

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/swupdate.cfg.in ${D}${sysconfdir}/swupdate.cfg
    sed -i "s#@BOARD_NAME@#${OTA_BOARD_NAME}#g" ${D}${sysconfdir}/swupdate.cfg
}

FILES:${PN}:append = " \
    ${sysconfdir}/swupdate.cfg \
    ${libdir}/swupdate/conf.d/09-swupdate-args \
"
