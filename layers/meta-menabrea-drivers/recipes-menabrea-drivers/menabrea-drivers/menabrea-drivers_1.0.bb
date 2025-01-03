SUMMARY = "An out-of-tree kernel modules recipe"
DESCRIPTION = "Out-of-tree kernel modules"
LICENSE = "MIT"

SRC_URI = " \
    file://drivers/kernel \
    "

S = "${WORKDIR}/drivers/kernel"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit module

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

RPROVIDES:${PN} += "kernel-module-mring"

# Automatically load kernel modules at boot
KERNEL_MODULE_AUTOLOAD += "mring"

KERNEL_MODULE_PROBECONF += "mring"
module_conf_mring = "options mring order=4 period=2000 burst=256"
