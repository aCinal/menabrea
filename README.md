# menabrea

Named after Luigi Federico Menabrea, a pioneer in the field of parallel computing and the study of overconstrained systems.

## Usage

### Environment setup

Bootstrap the repository (fetch a reference distribution, BSP metalayer, etc.)

```bash
./bootstrap
```

Initialize the work environment (needs to be done every time a new shell/session is used)

```bash
. initialize
```

---

### Using QEMU

Build and run a test QEMU image...

```bash
MACHINE=qemuarm64 DISTRO=menabrea-qemu bitbake menabrea-image
runqemu qemuarm64 nographic
```

Alternatively, the variables `MACHINE` and `DISTRO` can be set in `build/conf/local.conf` in which case the command is only `bitbake menabrea-image`.

---

### Deploying to a rack of Raspberry Pi 4's

Build a Raspberry Pi 4 image:

```bash
MACHINE=raspberrypi4-64 DISTRO=menabrea-node1 bitbake menabrea-image
```

Flash the microSD card:

```bash
IMG_NAME="menabrea-image-raspberrypi4-64.wic"
# Copy the compressed image to /tmp
cp ./tmp/deploy/images/raspberrypi4-64/${IMG_NAME}.bz2 /tmp
# Decompress (overwrite existing file if present)
bunzip2 /tmp/${IMG_NAME}.bz2 -f
# Flash the card
sudo dd if=/tmp/${IMG_NAME} of=<microSD card device>
```

where `<microSD card device>` is, e.g. `/dev/mmcblk0` or `/dev/sda`.

Repeat the steps for the remaining nodes:

```bash
MACHINE=raspberrypi4-64 DISTRO=menabrea-node2 bitbake menabrea-image
MACHINE=raspberrypi4-64 DISTRO=menabrea-node3 bitbake menabrea-image
```

Connect to the board via serial connection, e.g. using `picocom`:

```bash
sudo picocom -b 115200 -l -r <USB-to-serial device>
```

where `<USB-to-serial device>` is, e.g. `/dev/ttyUSB0`.

---

### Development build

To enable development features such as the serial console and an SSH server, add the following line in `build/conf/local.conf`:

```
OVERRIDES =. "menabrea-dev:"
```

When using the development build the platform is not started automatically, allowing the developer to log in to the Raspberry Pi, modify the configuration options in `/opt/platform_config.json` and start the platform manually:

```bash
systemctl start menabrea
```

---

### Application deployment

Applications run on top of the platform are provided as shared libraries and loaded by the platform at startup. An application must export the following symbols (with C linkage):

- `ApplicationGlobalInit` will be called by the platform during startup before forking the event dispatchers. It gives the application an opportunity to do global initialization, e.g. set up shared memory in a way that allows passing pointers directly between processes by ensuring identical address space layouts in all processes. All platform APIs can already be used at this point (except where explicitly noted).
- `ApplicationLocalInit` will be called once in each dispatcher process during the platform startup.
- `ApplicationLocalExit` will be called once in each dispatcher process during the platform shutdown.
- `ApplicationGlobalExit` will be called by the platform during shutdown after local exits complete in all dispatchers ("EM cores"). All platform APIs can still be used at this point (except where explicitly noted).

Applications should define these symbols using the following macros found in `menabrea/common.h`:
- `APPLICATION_GLOBAL_INIT()`
- `APPLICATION_LOCAL_INIT(core)`
- `APPLICATION_LOCAL_EXIT(core)`
- `APPLICATION_GLOBAL_EXIT()`

Applications to be loaded are selected via the `platform_config.json` files. The startup script (`menabrea.py`) parses the JSON file and sets the environment variable `MENABREA_APP_LIST` appropriately.
