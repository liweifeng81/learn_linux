# Bootloader & U-Boot Notes
# ==========================
# Interview Reference — Embedded Linux Boot Sequence

## Linux Boot Sequence (ARM SoC)

```
Power-on Reset
     │
     ▼
┌─────────────┐
│  ROM Code   │  — Hardwired in SoC (cannot be changed)
│  (First     │    Initialises minimal clock/SRAM
│  bootloader)│    Loads next stage from SD/eMMC/NAND/USB
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  SPL / MLO  │  — Secondary Program Loader (fits in SRAM ~256KB)
│ (U-Boot SPL)│    Initialises DRAM controller
│             │    Loads full U-Boot into DRAM
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   U-Boot    │  — Full bootloader in DRAM
│             │    Sets up peripherals (Ethernet, USB, NAND)
│             │    Provides interactive console (bootdelay)
│             │    Loads kernel + DTB + initramfs
│             │    Passes bootargs to kernel
└──────┬──────┘
       │   bootm / bootz / booti
       ▼
┌─────────────┐
│Linux Kernel │  — Decompresses, initialises MMU, BSS, interrupts
│             │    Mounts rootfs, runs /sbin/init or systemd
└─────────────┘
```

## Key U-Boot Commands

| Command | Description |
|---------|-------------|
| `printenv` | Print all environment variables |
| `setenv bootargs "console=ttyS0,115200 root=/dev/mmcblk0p2"` | Set kernel command line |
| `saveenv` | Save environment to flash |
| `mmc list` / `mmc rescan` | List / rescan MMC devices |
| `fatload mmc 0:1 0x80000000 zImage` | Load file from FAT to memory |
| `ext4load mmc 0:2 0x82000000 /boot/zImage` | Load from ext4 filesystem |
| `bootz 0x80000000 - 0x83000000` | Boot ARM zImage with DTB at addr |
| `booti 0x80000000 - 0x83000000` | Boot ARM64 Image |
| `bootm 0x80000000` | Boot legacy uImage |
| `tftp 0x80000000 zImage` | Load from TFTP server |
| `md 0x80000000 0x10` | Memory display (hex dump) |
| `mw 0x80000000 0xdeadbeef` | Memory write |
| `nm 0x80000000` | Memory modify (interactive) |

## Important environment variables

```bash
# Standard boot flow
setenv bootcmd 'mmc dev 0; ext4load mmc 0:1 ${kernel_addr_r} /boot/zImage; \
                ext4load mmc 0:1 ${fdt_addr_r} /boot/my-board.dtb; \
                bootz ${kernel_addr_r} - ${fdt_addr_r}'

# Root filesystem on SD card
setenv bootargs 'console=ttyS0,115200n8 root=/dev/mmcblk0p2 rw rootwait'

# NFS root (development/debug)
setenv bootargs 'console=ttyS0,115200 root=/dev/nfs nfsroot=192.168.1.100:/srv/nfs/rootfs \
                 ip=dhcp rw rootwait'

saveenv
```

## mkimage — Creating U-Boot Images

```bash
# Wrap a Linux kernel as a uImage
mkimage -A arm -O linux -T kernel -C none \
        -a 0x80008000 -e 0x80008000 \
        -n "Linux Kernel" -d zImage uImage

# Wrap a DTB as FDT image
mkimage -A arm -O linux -T flat_dt -C none \
        -a 0x83000000 -e 0x83000000 \
        -n "Device Tree" -d my-board.dtb my-board.dtb.uimg

# Create a U-Boot script from a text file
mkimage -A arm -O linux -T script -C none \
        -n "Boot Script" -d boot.txt boot.scr
```

## Device Tree Basics

```dts
/* my-board.dts */
/dts-v1/;
#include "soc.dtsi"

/ {
    model = "My Embedded Board";
    compatible = "vendor,my-board", "vendor,soc-family";

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x20000000>; /* 512 MB */
    };

    chosen {
        bootargs = "console=ttyS0,115200";
    };

    leds {
        compatible = "gpio-leds";
        status-led {
            gpios = <&gpio 17 GPIO_ACTIVE_HIGH>;
            default-state = "on";
        };
    };
};

/* Extend UART node from dtsi */
&uart0 {
    status = "okay";
};
```

## Interview Q&A

**Q: What is a device tree?**
A: A hardware description passed by the bootloader to the kernel.
   Describes SoC peripherals (memory base, IRQ numbers, clock names) so the
   kernel doesn't need board-specific #ifdefs. File: .dts → dtc → .dtb.

**Q: What is SPL?**
A: Secondary Program Loader — a minimal U-Boot that fits in the SoC's
   on-chip SRAM. It initializes DRAM, then loads the full U-Boot.

**Q: How does U-Boot pass parameters to the Linux kernel?**
A: Via `bootargs` environment variable → `/proc/cmdline`.
   Also passes the DTB address in a register (r2 on ARM32, x0 on ARM64).

**Q: What is DFU / UMS mode?**
A: Device Firmware Upgrade / USB Mass Storage — U-Boot can expose flash
   as a USB storage device for easy firmware updates without JTAG.
