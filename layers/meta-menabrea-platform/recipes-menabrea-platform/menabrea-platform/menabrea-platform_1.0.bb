SUMMARY = "Application platform recipe"
DESCRIPTION = "Application platform"
LICENSE = "MIT"

SRC_URI = "file://platform"

S = "${WORKDIR}/platform"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    em-odp \
    "

PLATFORM_CONFIG_PATH:menabrea-node1 := "node1/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-node2 := "node2/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-node3 := "node3/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-qemu := "qemu/platform_config.json"
SRC_URI += "file://${PLATFORM_CONFIG_PATH}"

do_install:append() {

    install -d ${D}/opt
    install -m 755 ${WORKDIR}/${PLATFORM_CONFIG_PATH} ${D}/opt
}

FILES:${PN} += "/opt"

inherit cmake
