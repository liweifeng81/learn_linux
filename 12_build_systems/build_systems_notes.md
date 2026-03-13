# Build Systems: Buildroot & Yocto
# ==================================
# Interview Reference — Embedded Linux Build Frameworks

---

## Buildroot

### What is Buildroot?
- Generates a **complete embedded Linux system** (toolchain, kernel, rootfs, bootloader)
  from a single `make` command.
- Uses **Kconfig** for configuration (same system as the Linux kernel).
- Simpler than Yocto; suited for small–medium complexity products.
- Output: `output/images/` — rootfs.tar.gz, sdcard.img, zImage, dtbs, etc.

### Key Directory Structure
```
buildroot/
├── configs/         # Defconfigs (e.g. raspberrypi4_defconfig)
├── package/         # Package definitions (*.mk + Config.in)
├── board/           # Board-specific overlay files & scripts
├── dl/              # Downloaded source tarballs (cache)
├── output/
│   ├── build/       # Per-package build directories
│   ├── host/        # Cross-compilation toolchain
│   ├── target/      # Root filesystem staging area
│   └── images/      # Final images (flash-ready)
└── Makefile
```

### Quick Start
```bash
cd buildroot
make raspberrypi4_64_defconfig   # load a board defconfig
make menuconfig                   # customize (BR2_ options)
make linux-menuconfig             # kernel config
make busybox-menuconfig           # busybox config
make -j$(nproc)                  # build everything
ls output/images/
```

### Adding a Custom Package
```bash
# 1. Create package directory
mkdir -p package/my-app

# 2. Config.in (Kconfig entry)
cat > package/my-app/Config.in << 'EOF'
config BR2_PACKAGE_MY_APP
    bool "my-app"
    help
      My custom application demo.
EOF

# 3. my-app.mk (package recipe — Makefile fragment)
cat > package/my-app/my-app.mk << 'EOF'
MY_APP_VERSION = 1.0
MY_APP_SITE    = $(BR2_EXTERNAL_MY_APPS_PATH)/my-app
MY_APP_SITE_METHOD = local

define MY_APP_BUILD_CMDS
    $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

define MY_APP_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/my-app $(TARGET_DIR)/usr/bin/my-app
endef

$(eval $(generic-package))
EOF

# 4. Add to package/Config.in
echo 'source "package/my-app/Config.in"' >> package/Config.in

# 5. Enable in menuconfig
make menuconfig   # find and enable "my-app"
make             # rebuild
```

### Board Overlay (Custom Files on Target)
```bash
# board/myboard/rootfs-overlay/
# Files here are copied verbatim into the rootfs
mkdir -p board/myboard/rootfs-overlay/etc
echo "nameserver 8.8.8.8" > board/myboard/rootfs-overlay/etc/resolv.conf

# In menuconfig: System configuration → Root filesystem overlay directories
# → board/myboard/rootfs-overlay
```

---

## Yocto Project

### What is Yocto?
- Industry-standard framework for building **custom embedded Linux distributions**.
- More complex and flexible than Buildroot; supports complex multi-layer projects.
- Based on **OpenEmbedded** (BitBake build engine + metadata).
- Output: complete OS images, SDK, package feeds (RPM/DEB/IPK).

### Key Concepts

| Term | Description |
|------|-------------|
| **BitBake** | Build engine (like make, but task-based) |
| **Recipe** (`.bb`) | Defines how to fetch, configure, compile, install one package |
| **Layer** (`meta-*`) | Collection of recipes and config for a theme/board/feature |
| **Image** | A recipe that assembles recipes into a rootfs image |
| **Class** (`.bbclass`) | Reusable recipe logic (e.g. `cmake`, `autotools`) |
| **MACHINE** | Target hardware (e.g. `raspberrypi4-64`) |
| **DISTRO** | Linux distribution policy (e.g. `poky`, `nodistro`) |

### Layer Structure
```
meta-my-layer/
├── conf/
│   └── layer.conf          # Layer configuration
├── recipes-core/
│   └── images/
│       └── my-image.bb     # Custom image recipe
├── recipes-apps/
│   └── my-app/
│       ├── my-app_1.0.bb   # Application recipe
│       └── files/          # Local files/patches
└── README
```

### Quick Start (Poky)
```bash
git clone git://git.yoctoproject.org/poky
source poky/oe-init-build-env build/   # sets up build/ dir, activates bitbake

# Edit build/conf/local.conf
MACHINE ?= "raspberrypi4-64"
DISTRO  ?= "poky"

# Build core image
bitbake core-image-minimal        # minimal console image
bitbake core-image-full-cmdline   # with networking tools
bitbake core-image-sato           # with GUI

# Add a recipe to your image
IMAGE_INSTALL:append = " my-app openssh"

# Build SDK
bitbake -c populate_sdk core-image-minimal
```

### Writing a Recipe
```bitbake
# recipes-apps/my-app/my-app_1.0.bb
SUMMARY       = "My custom application"
DESCRIPTION   = "Demo app for embedded Linux interview"
LICENSE       = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=<hash>"

SRC_URI = "git://github.com/user/my-app.git;protocol=https;branch=main"
SRCREV  = "${AUTOREV}"

# Use CMake class
inherit cmake

# Install step (default cmake install is usually sufficient)
do_install:append() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/my-app ${D}${bindir}/my-app
}
```

### Interview Q&A

**Q: Buildroot vs Yocto?**
| | Buildroot | Yocto |
|--|-----------|-------|
| Learning curve | Low | High |
| Flexibility | Medium | Very high |
| Binary package support | No | Yes (RPM/DEB/IPK) |
| Reproducible builds | Good | Excellent |
| Industry adoption | Hobbyist / SME | Enterprise |
| Build time | Fast | Slow (first build) |

**Q: What is a Yocto layer?**
A: A logical grouping of recipes, configuration, and classes for a
   specific purpose (BSP, application, distro policy). Layers allow
   modular sharing and override without modifying upstream layers.

**Q: What is BitBake?**
A: The task execution engine of Yocto/OpenEmbedded. Reads recipe files,
   resolves dependencies, and executes tasks (fetch, unpack, configure,
   compile, install, package) in the correct order, with caching.

**Q: How do you add a kernel patch in Yocto?**
A: In a BSP layer recipe that extends linux-yocto:
```bitbake
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://0001-my-driver-fix.patch"
```
