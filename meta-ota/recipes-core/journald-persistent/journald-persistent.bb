SUMMARY = "Lưu journald bền vững trên /data với rotation"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://10-persistent.conf \
    file://journald-data.conf \
    file://journald-data-flush.service \
"

S = "${WORKDIR}"

inherit systemd

RDEPENDS:${PN} = "systemd"

SYSTEMD_SERVICE:${PN} = "journald-data-flush.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${sysconfdir}/systemd/journald.conf.d
    install -m 0644 ${WORKDIR}/10-persistent.conf \
        ${D}${sysconfdir}/systemd/journald.conf.d/10-persistent.conf

    install -d ${D}${sysconfdir}/tmpfiles.d
    install -m 0644 ${WORKDIR}/journald-data.conf \
        ${D}${sysconfdir}/tmpfiles.d/journald-data.conf

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/journald-data-flush.service \
        ${D}${systemd_system_unitdir}/journald-data-flush.service
}

FILES:${PN} = " \
    ${sysconfdir}/systemd/journald.conf.d/10-persistent.conf \
    ${sysconfdir}/tmpfiles.d/journald-data.conf \
    ${systemd_system_unitdir}/journald-data-flush.service \
"
