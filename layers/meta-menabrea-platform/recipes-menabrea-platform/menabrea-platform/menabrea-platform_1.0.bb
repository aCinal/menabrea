SUMMARY = "Application platform recipe"
DESCRIPTION = "Application platform"
LICENSE = "MIT"

SRC_URI = " \
    file://platform \
    file://${PLATFORM_CONFIG_PATH} \
    file://menabrea.service \
    "

S = "${WORKDIR}/platform"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    em-odp \
    menabrea-drivers-libs \
    "

RDEPENDS:${PN} = " \
    python3 \
    em-odp \
    menabrea-drivers-libs \
    "

inherit cmake systemd

PLATFORM_CONFIG_PATH:menabrea-node1 := "node1/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-node2 := "node2/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-node3 := "node3/platform_config.json"
PLATFORM_CONFIG_PATH:menabrea-qemu  := "qemu/platform_config.json"

FILES:${PN} += "/opt"
FILES:${PN} += "${systemd_unitdir}/system/menabrea.service"

SYSTEMD_SERVICE:${PN} = "${@bb.utils.contains('MENABREA_AUTOSTART', '1', 'menabrea.service', '', d)}"

do_install:append() {

    install -d ${D}/opt
    install -m 0644 ${WORKDIR}/${PLATFORM_CONFIG_PATH} ${D}/opt

    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/menabrea.service ${D}/${systemd_unitdir}/system
}
