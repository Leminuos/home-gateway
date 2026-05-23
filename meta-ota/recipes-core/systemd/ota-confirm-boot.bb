LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://ota-confirm-boot.sh \
    file://ota-confirm-boot.service \
"

S = "${WORKDIR}"

inherit systemd

RDEPENDS:${PN} = "libubootenv-bin systemd"

SYSTEMD_SERVICE:${PN}     = "ota-confirm-boot.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/ota-confirm-boot.sh ${D}${bindir}/ota-confirm-boot.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/ota-confirm-boot.service ${D}${systemd_system_unitdir}/ota-confirm-boot.service
}

FILES:${PN} = " \
    ${bindir}/ota-confirm-boot.sh \
    ${systemd_system_unitdir}/ota-confirm-boot.service \
"
