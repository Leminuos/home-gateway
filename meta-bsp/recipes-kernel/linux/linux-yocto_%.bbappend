FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://0001-bbb-home-gateway-dts.patch"
SRC_URI += "file://bbb_home_gateway.cfg"

KERNEL_CONFIG_FRAGMENTS:append = " ${WORKDIR}/bbb_home_gateway.cfg"

COMPATIBLE_MACHINE:append = "|bbb-home-gateway"
KMACHINE:bbb-home-gateway = "beaglebone"
PREFERRED_PROVIDER_virtual/kernel ?= "linux-yocto"