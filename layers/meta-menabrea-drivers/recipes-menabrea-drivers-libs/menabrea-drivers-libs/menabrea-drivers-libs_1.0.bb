SUMMARY = "A recipe for user libraries for interfacing with out-of-tree kernel modules"
DESCRIPTION = "Userspace libraries for interfacing with out-of-tree kernel modules"
LICENSE = "MIT"

SRC_URI = " \
    file://drivers/ \
    "

S = "${WORKDIR}/drivers"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    menabrea-drivers \
    "

inherit cmake
