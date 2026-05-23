SUMMARY = "Qt Smart Home Dashboard"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/../../..:"

SRC_URI = "file://home-gateway-app \
           file://home-dashboard.service"

S = "${WORKDIR}/home-gateway-app"

inherit cmake_qt5 systemd

DEPENDS += " qtbase qtdeclarative qtsvg libgpiod mosquitto"

SYSTEMD_SERVICE:${PN} = "home-dashboard.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/home-gateway-app ${D}${bindir}/home-gateway-app

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/home-dashboard.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${bindir}/home-gateway-app"
FILES:${PN} += "${systemd_system_unitdir}/home-dashboard.service"
