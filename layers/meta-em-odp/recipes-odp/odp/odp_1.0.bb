SUMMARY = "OpenDataPlane recipe"
DESCRIPTION = "OpenDataPlane recipe"
LICENSE = "MIT"

SRC_URI = "git://github.com/OpenDataPlane/odp.git;branch=master;protocol=https"
SRCREV = "c8690d3e9dccc29fc9ac4005258d397abe8d4859"

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

# Uncomment to enable debug logs at ODP level
# EXTRA_OECONF:append = " --enable-debug-print"
