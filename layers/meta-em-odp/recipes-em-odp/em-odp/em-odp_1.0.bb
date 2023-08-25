SUMMARY = "OpenEventMachine recipe"
DESCRIPTION = "OpenEventMachine implementation based on OpenDataPlane"
LICENSE = "MIT"

SRC_URI = "git://github.com/openeventmachine/em-odp.git;branch=master;protocol=https"
SRCREV = "8ad178851cc8dd01e5387336b58f5d6940aef8a3"

# Git repositories are cloned to ${WORKDIR}/git by default
S = "${WORKDIR}/git"

LIC_FILES_CHKSUM = "file://README;md5=199ef336df89a30dc74d11cdcad6024f"

DEPENDS = " \
    libconfig \
    odp \
    "

inherit autotools pkgconfig

EXTRA_OECONF:append = " --without-programs"
EXTRA_OECONF:append = " --enable-lto"

# Uncomment to enable debug logs at EM level
# EXTRA_OECONF:append = " --enable-debug-print"
