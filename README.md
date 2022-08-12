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

The platform is started automatically as a systemd service. To prevent this set `MENABREA_AUTOSTART = "0"` in your `build/conf/local.conf` and rebuild the image. You can then modify configuration options in `/opt/platform_config.json` and start the platform manually:

```bash
python3 /opt/menabrea.py
```

## Application deployment

Applications run on top of the platform are provided as shared libraries and loaded by the platform at startup. An application must export the following symbols (with C linkage):

- `ApplicationGlobalInit` will be called by the platform during startup before forking the event dispatchers. It gives the application an opportunity to do global initialization, e.g. set up shared memory in a way that allows passing pointers directly between processes by ensuring identical address space layouts in all processes.
- `ApplicationLocalInit` will be called once in each dispatcher process during the platform startup.
- `ApplicationLocalExit` will be called once in each dispatcher process during the platform shutdown.
- `ApplicationGlobalExit` will be called by the platform during shutdown after local exits complete in all dispatchers ("EM cores"). All platform APIs can still be used at this point (except where explicitly noted).

Applications to be loaded are selected via the `platform_config.json` files. The startup script (`menabrea.py`) parses the JSON file and sets the environment variable `MENABREA_APP_LIST` appropriately.
