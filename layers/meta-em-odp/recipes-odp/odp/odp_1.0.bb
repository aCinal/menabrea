SUMMARY = "OpenDataPlane recipe"
DESCRIPTION = "OpenDataPlane recipe"
LICENSE = "MIT"

SRC_URI = " \
    git://github.com/OpenDataPlane/odp.git;branch=master;protocol=https \
    file://odp-menabrea.conf \
    "
SRCREV = "6c3ff3558708f248e574fafd6d3eab40b48c4595"

# Git repositories are cloned to ${WORKDIR}/git by default
S = "${WORKDIR}/git"

LIC_FILES_CHKSUM = "file://LICENSE;md5=8ab56d375d2d299f18b6c19f0ab29437"

DEPENDS = " \
    libconfig \
    openssl \
    "

inherit autotools pkgconfig

EXTRA_OECONF:append = " --without-examples"
EXTRA_OECONF:append = " --without-tests"
EXTRA_OECONF:append = " --enable-lto"

# Uncomment to enable debug logs at ODP level
# EXTRA_OECONF:append = " --enable-debug-print"

FILES:${PN} += "/opt"

do_install:append() {

    install -d ${D}/opt
    install -m 0755 ${WORKDIR}/odp-menabrea.conf ${D}/opt
}
