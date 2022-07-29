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
MACHINE=qemuarm64 bitbake core-image-minimal
runqemu qemuarm64 nographic
```

Start the platform (see `/opt/platform_config.json` for configuration options):

```bash
cd /opt
python3 menabrea.py
```
