SUMMARY = "Application platform recipe"
DESCRIPTION = "Application platform"
LICENSE = "MIT"

SRC_URI = "file://platform"

S = "${WORKDIR}/platform"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    em-odp \
    "

FILES_${PN} += "/opt"

inherit cmake
