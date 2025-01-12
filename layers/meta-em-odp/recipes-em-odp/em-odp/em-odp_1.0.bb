SUMMARY = "OpenEventMachine recipe"
DESCRIPTION = "OpenEventMachine implementation based on OpenDataPlane"
LICENSE = "MIT"

SRC_URI = "git://github.com/openeventmachine/em-odp.git;branch=master;protocol=https"
SRCREV = "d31ee7c13353a29f4e598ae91aa29e11a0bfa6d9"

# Git repositories are cloned to ${WORKDIR}/git by default
S = "${WORKDIR}/git"

LIC_FILES_CHKSUM = "file://README;md5=d6f51800777bcb4927c2d40847d823ee"

DEPENDS = " \
    libconfig \
    odp \
    "

inherit autotools pkgconfig

EXTRA_OECONF:append = " --without-programs"
EXTRA_OECONF:append = " --enable-lto"

# Uncomment to enable debug logs at EM level
# EXTRA_OECONF:append = " --enable-debug-print"
