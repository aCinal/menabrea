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

Build and run a test QEMU image...

```bash
MACHINE=qemuarm64 DISTRO=menabrea-qemu bitbake menabrea-image
runqemu qemuarm64 nographic
```

...or build images deployable to a rack of three Raspberry Pi 4's:

```bash
MACHINE=raspberrypi4-64 DISTRO=menabrea-node1 bitbake menabrea-image
MACHINE=raspberrypi4-64 DISTRO=menabrea-node2 bitbake menabrea-image
MACHINE=raspberrypi4-64 DISTRO=menabrea-node3 bitbake menabrea-image
```

The platform is started automatically as a systemd service. To prevent this set `MENABREA_AUTOSTART = "0"` in your `build/conf/local.conf` and rebuild the image. You can then modify configuration options in `/opt/menabrea/platform_config.json` and start the platform manually:

```bash
cd /opt
python3 menabrea.py
```
