# menabrea

Named after Luigi Federico Menabrea, a pioneer in the field of parallel computing and the study of overconstrained systems.

## Usage

Bootstrap the repository (fetch a reference distribution, BSP metalayer, etc.)

```bash
./bootstrap
```

Initialize the work environment

```bash
. initialize
```

Build and run a test QEMU image

```bash
MACHINE=qemuarm64 DISTRO=menabrea-qemu bitbake menabrea-image
runqemu qemuarm64 nographic
```

Start the platform (see `/opt/platform_config.json` for configuration options):

```bash
cd /opt
python3 menabrea.py
```

Build the images deployable to a rack of three Raspberry Pi 4's:

```bash
MACHINE=raspberrypi4-64 DISTRO=menabrea-node1 bitbake menabrea-image
MACHINE=raspberrypi4-64 DISTRO=menabrea-node2 bitbake menabrea-image
MACHINE=raspberrypi4-64 DISTRO=menabrea-node3 bitbake menabrea-image
```
