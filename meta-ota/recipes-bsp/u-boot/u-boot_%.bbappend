FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
    file://u-boot-ota-env.txt \
    file://0001-bbb-ota.cfg \
"

SRC_URI:append = " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', \
       'file://0001-bbb-ota-secure-boot.patch', \
       'file://0001-bbb-ota-normal-boot.patch', d)} \
"

# u-boot-tools-native cung cấp mkenvimage để build u-boot-env.raw
DEPENDS:append = " u-boot-tools-native"

do_compile:append() {
    cat ${WORKDIR}/u-boot-ota-env.txt >> ${B}/u-boot-initial-env
    mkenvimage -s 0x20000 -o ${B}/u-boot-env.raw ${B}/u-boot-initial-env
}

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${B}/u-boot-initial-env ${D}${sysconfdir}/u-boot-initial-env
}

do_deploy:append() {
    install -m 0644 ${B}/u-boot-env.raw ${DEPLOYDIR}/u-boot-env.raw
}
