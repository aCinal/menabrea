# Isolate cores 1-3 from the kernel scheduler
CMDLINE_append = " isolcpus=1,2,3"

# Use static IP during development
CMDLINE_append = " ip=192.168.137.2"
