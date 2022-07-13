SUMMARY = "Dummy application"
DESCRIPTION = "Dummy application recipe"
LICENSE = "MIT"

SRC_URI = "file://application"

S = "${WORKDIR}/application"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    menabrea-platform \
    "

inherit cmake
