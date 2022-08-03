require recipes-core/images/core-image-minimal.bb

SUMMARY = "Menabrea image"
DESCRIPTION = "Menabrea production image"
LICENSE = "MIT"

CORE_IMAGE_EXTRA_INSTALL += "menabrea-app"
CORE_IMAGE_EXTRA_INSTALL += "menabrea-platform"
CORE_IMAGE_EXTRA_INSTALL += "menabrea-drivers"
CORE_IMAGE_EXTRA_INSTALL += "em-odp"
CORE_IMAGE_EXTRA_INSTALL += "python3"

# Automatically load kernel modules at boot
KERNEL_MODULE_AUTOLOAD += "pangloss-driver"
KERNEL_MODULE_AUTOLOAD += "dummy-ringbuffer"

# Use an SSH server during development
IMAGE_FEATURES += "ssh-server-openssh"
