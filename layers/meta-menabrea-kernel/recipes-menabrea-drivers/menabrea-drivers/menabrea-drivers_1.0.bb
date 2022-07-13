SUMMARY = "A dummy out-of-tree kernel module recipe"
DESCRIPTION = "A dummy kernel module"
LICENSE = "MIT"

SRC_URI = "file://drivers \
          "

S = "${WORKDIR}/drivers"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit module

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

RPROVIDES_${PN} += "kernel-module-pangloss-driver"
RPROVIDES_${PN} += "kernel-module-dummy-ringbuffer"
