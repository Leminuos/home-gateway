FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append:bbb-home-gateway = " file://fw_env.config"

do_install:append:bbb-home-gateway() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/fw_env.config ${D}${sysconfdir}/fw_env.config
}

FILES:${PN}:append:bbb-home-gateway = " ${sysconfdir}/fw_env.config"
