require recipes-core/images/core-image-minimal.bb

SUMMARY = "Menabrea image"
DESCRIPTION = "Menabrea production image"
DESCRIPTION:menabrea-dev = "Menabrea development image"
LICENSE = "MIT"

IMAGE_INSTALL:append              = " menabrea-app"
IMAGE_INSTALL:append              = " menabrea-drivers"
IMAGE_INSTALL:append              = " menabrea-drivers-libs"
IMAGE_INSTALL:append              = " menabrea-platform"
IMAGE_INSTALL:append              = " menabrea-platform-test"
IMAGE_INSTALL:append              = " menabrea-test-harness"
IMAGE_INSTALL:append:menabrea-dev = " tcpdump"
IMAGE_INSTALL:append:menabrea-dev = " ethtool"

# Use an SSH server during development
IMAGE_FEATURES:append:menabrea-dev = " ssh-server-openssh"
