# Base this distribution on poky
require conf/distro/poky.conf

# Remove unused distro features
DISTRO_FEATURES:remove = \
    "acl alsa bluetooth debuginfod usbgadget usbhost wifi nfs zeroconf 3g nfc x11 opengl wayland vulkan pulseaudio gobject-introspection-data"

# Use systemd as init manager
INIT_MANAGER = "systemd"

# Add sources and config directories
FILESPATH:append = ":${TOPDIR}/../src"
FILESPATH:append = ":${TOPDIR}/../config"

# Let systemd start the platform at boot by default, unless running a development build
MENABREA_AUTOSTART              = "1"
MENABREA_AUTOSTART:menabrea-dev = "0"

# Enable UART on the Raspberry Pi 4 in the development image
ENABLE_UART:menabrea-dev := "1"
