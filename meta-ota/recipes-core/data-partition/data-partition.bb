SUMMARY = "Data partition: pre-populated ext4 image + systemd mount unit"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://mqtt.json    \
    file://ota.json     \
    file://setting.json \
    file://data.mount   \
"

# e2fsprogs-native cung cấp mke2fs để build ext4 image tại build time
DEPENDS = "e2fsprogs-native"

inherit systemd deploy

SYSTEMD_SERVICE:${PN} = "data.mount"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

# 128 MiB = 131072 KiB (khớp với --fixed-size 128 trong bbb-ota.wks)
DATA_PARTITION_SIZE_KiB = "131072"

do_compile[cleandirs] = "${WORKDIR}/data-root"

do_compile() {
    install -d ${WORKDIR}/data-root/journal
    
    install -d ${WORKDIR}/data-root/config
    install -m 0644 ${WORKDIR}/mqtt.json    ${WORKDIR}/data-root/config/mqtt.json
    install -m 0644 ${WORKDIR}/ota.json     ${WORKDIR}/data-root/config/ota.json
    install -m 0644 ${WORKDIR}/setting.json ${WORKDIR}/data-root/config/setting.json
    
    mke2fs -t ext4 -L data \
           -d ${WORKDIR}/data-root \
           ${WORKDIR}/data-home-gateway.ext4 \
           ${DATA_PARTITION_SIZE_KiB}
}

do_install() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/data.mount ${D}${systemd_system_unitdir}/data.mount
}

addtask deploy after do_compile before do_build

do_deploy() {
    install -Dm 0644 ${WORKDIR}/data-home-gateway.ext4 \
        ${DEPLOYDIR}/data-home-gateway.ext4
}

FILES:${PN} = "${systemd_system_unitdir}/data.mount"
