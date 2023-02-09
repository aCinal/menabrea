SUMMARY = "OpenEventMachine recipe"
DESCRIPTION = "OpenEventMachine implementation based on OpenDataPlane"
LICENSE = "MIT"

SRC_URI = "git://github.com/openeventmachine/em-odp.git;branch=master;protocol=https"
SRCREV = "f3de2d2f3235e37c6f13f839b8024ba97c5b5ef2"

# Git repositories are cloned to ${WORKDIR}/git by default
S = "${WORKDIR}/git"

LIC_FILES_CHKSUM = "file://README;md5=e9afc838f39bc2d3bd037562fab7e6e0"

DEPENDS = " \
    libconfig \
    odp \
    "

inherit autotools pkgconfig

EXTRA_OECONF:append = " --without-programs"

# Uncomment to enable debug logs at EM level
# EXTRA_OECONF:append = " --enable-debug-print"
