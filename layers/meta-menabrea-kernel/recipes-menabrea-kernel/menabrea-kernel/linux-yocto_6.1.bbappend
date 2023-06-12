FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += " \
    file://menabrea.cfg \
    file://0001-fix-isolcpus-uninitialized-mask-read.patch \
    "
