require recipes-core/images/core-image-minimal.bb

SUMMARY = "Menabrea image"
DESCRIPTION = "Menabrea production image"
DESCRIPTION:menabrea-dev = "Menabrea development image"
LICENSE = "MIT"

CORE_IMAGE_EXTRA_INSTALL:append              = " menabrea-app"
CORE_IMAGE_EXTRA_INSTALL:append              = " menabrea-drivers"
CORE_IMAGE_EXTRA_INSTALL:append              = " menabrea-platform"
CORE_IMAGE_EXTRA_INSTALL:append              = " menabrea-platform-test"
CORE_IMAGE_EXTRA_INSTALL:append              = " em-odp"
CORE_IMAGE_EXTRA_INSTALL:append              = " python3"
CORE_IMAGE_EXTRA_INSTALL:append:menabrea-dev = " tcpdump"

# Automatically load kernel modules at boot
KERNEL_MODULE_AUTOLOAD:append = " pangloss-driver"
KERNEL_MODULE_AUTOLOAD:append = " dummy-ringbuffer"

# Use an SSH server during development
IMAGE_FEATURES:append:menabrea-dev = " ssh-server-openssh"
