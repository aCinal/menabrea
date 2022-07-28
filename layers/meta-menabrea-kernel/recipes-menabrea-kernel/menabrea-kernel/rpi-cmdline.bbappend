# Isolate cores 1-3 from the kernel scheduler (note that this can also
# be achieved by setting the ISOLATED_CPUS variable - rpi-cmdline.bb
# will then append the corresponding entry to the kernel command line)
CMDLINE:append = " isolcpus=1,2,3"

# Use static IP during development
CMDLINE:append = " ip=192.168.137.2"
