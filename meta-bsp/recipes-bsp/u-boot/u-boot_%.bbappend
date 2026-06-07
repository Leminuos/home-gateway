FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Fragment chỉ được apply khi secure-boot được bật
SRC_URI:append = " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'secure-boot', 'file://verified-boot.cfg', '', d)} \
"
