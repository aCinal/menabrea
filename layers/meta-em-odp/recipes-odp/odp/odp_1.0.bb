SUMMARY = "OpenDataPlane recipe"
DESCRIPTION = "OpenDataPlane recipe"
LICENSE = "MIT"

SRC_URI = " \
    git://github.com/OpenDataPlane/odp.git;branch=master;protocol=https \
    file://odp-menabrea.conf \
    "
SRCREV = "875d5bb720580f4695818dfc53e2bc38e215159e"

# Git repositories are cloned to ${WORKDIR}/git by default
S = "${WORKDIR}/git"

LIC_FILES_CHKSUM = "file://LICENSE;md5=18f96fe45a17e028d7b62e6191fcc675"

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
