# Use systemd as init manager
VIRTUAL-RUNTIME_init_manager = "systemd"
DISTRO_FEATURES += "systemd"
# Prevent the SysVinit distribution feature from being automatically enabled
DISTRO_FEATURES_BACKFILL_CONSIDERED = "sysvinit"

# Add sources and config directories
FILESPATH:append = ":${TOPDIR}/../src"
FILESPATH:append = ":${TOPDIR}/../config"

# Enable UART on the Raspberry Pi 4
ENABLE_UART := "1"
