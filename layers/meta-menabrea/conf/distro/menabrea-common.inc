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
