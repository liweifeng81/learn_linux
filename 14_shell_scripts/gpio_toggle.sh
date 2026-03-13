#!/bin/bash
# 14_shell_scripts/gpio_toggle.sh
# ================================
# Demonstrates GPIO control via Linux sysfs GPIO interface (legacy)
# and the modern libgpiod / gpiod tools approach.
#
# Sysfs GPIO (/sys/class/gpio) is deprecated since kernel 4.8 but still
# widely used in embedded systems. The new API uses /dev/gpiochipN.
#
# Interview topics:
#   Q: How do you control a GPIO from userspace in Linux?
#   A: Legacy: sysfs /sys/class/gpio/export — simple but deprecated.
#      Modern: libgpiod (gpioset, gpioget, gpiomon) or ioctl on /dev/gpiochipN.
#      From kernel modules: gpio_request / gpio_direction_output / gpio_set_value.
#
#   Q: What is pinmux/pinctrl?
#   A: Hardware multiplexer that selects the function of a pin (GPIO, UART,
#      SPI, etc.). Configured via device tree pinctrl entries.
#
#   Q: What is device tree?
#   A: Hardware description language used by Linux ARM/AArch64 systems.
#      Tells the kernel about SoC peripherals, memory map, IRQs, clocks.
#      Lives in .dts/.dtsi files, compiled to .dtb by dtc.

GPIO_NUM="${1:-17}"   # Default: GPIO 17 (BCM numbering, Raspberry Pi)
SYSFS_GPIO="/sys/class/gpio"

echo "=== GPIO Toggle Demo ==="
echo "GPIO number : $GPIO_NUM"

# ───────────────────────────────────────────────
# Legacy sysfs GPIO approach
# ───────────────────────────────────────────────
sysfs_gpio_demo() {
    local gpio=$1

    echo "--- Legacy sysfs GPIO ---"

    # Check if GPIO already exported
    if [[ ! -d "$SYSFS_GPIO/gpio${gpio}" ]]; then
        echo "Exporting GPIO $gpio"
        echo "$gpio" > "$SYSFS_GPIO/export" 2>/dev/null || {
            echo "Cannot export GPIO $gpio (no hardware or permission denied)"
            return 1
        }
    fi

    echo "out" > "$SYSFS_GPIO/gpio${gpio}/direction"
    echo "Direction set to: output"

    for i in 1 0 1 0 1; do
        echo "$i" > "$SYSFS_GPIO/gpio${gpio}/value"
        echo "GPIO $gpio = $i"
        sleep 0.5
    done

    # Cleanup
    echo "$gpio" > "$SYSFS_GPIO/unexport" 2>/dev/null
    echo "GPIO $gpio unexported"
}

# ───────────────────────────────────────────────
# Modern: gpiod tools (requires libgpiod-tools)
# ───────────────────────────────────────────────
gpiod_demo() {
    local gpio=$1
    echo ""
    echo "--- Modern gpiod API ---"

    if ! command -v gpioset &>/dev/null; then
        echo "gpioset not found — install: apt install gpiod"

        echo "Commands that would be run:"
        echo "  gpiodetect                          # list all gpiochip devices"
        echo "  gpioinfo gpiochip0                  # show all lines on chip 0"
        echo "  gpioget  gpiochip0 $gpio             # read GPIO $gpio"
        echo "  gpioset  gpiochip0 $gpio=1           # set GPIO $gpio HIGH"
        echo "  gpioset  gpiochip0 $gpio=0           # set GPIO $gpio LOW"
        echo "  gpiomon  --num-events=5 gpiochip0 $gpio  # monitor GPIO $gpio for 5 events"
        return
    fi

    echo "Available GPIO chips:"
    gpiodetect

    echo "Setting GPIO $gpio HIGH then LOW (gpiochip0):"
    gpioset gpiochip0 "${gpio}=1"
    sleep 0.5
    gpioset gpiochip0 "${gpio}=0"
    echo "Done"
}

# ───────────────────────────────────────────────
# /proc/interrupts for GPIO IRQs
# ───────────────────────────────────────────────
irq_demo() {
    echo ""
    echo "--- GPIO IRQ lines in /proc/interrupts ---"
    grep -i gpio /proc/interrupts 2>/dev/null | head -10 || echo "(no GPIO IRQ entries found)"
}

# Run demos
sysfs_gpio_demo "$GPIO_NUM" || true
gpiod_demo "$GPIO_NUM"
irq_demo

echo ""
echo "=== GPIO Demo Complete ==="
