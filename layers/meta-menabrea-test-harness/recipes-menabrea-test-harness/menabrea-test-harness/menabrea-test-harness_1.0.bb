SUMMARY = "Regression test framework recipe"
DESCRIPTION = "Regression test framework recipe"
LICENSE = "MIT"

SRC_URI = "file://test-harness"

S = "${WORKDIR}/test-harness"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    menabrea-platform \
    "

inherit cmake

FILES:${PN} += "/opt"
