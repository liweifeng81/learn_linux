#!/bin/bash
# 14_shell_scripts/sys_info.sh
# ============================
# System information script for embedded Linux boards.
# Useful for quick hardware/OS characterization on a new target.

echo "╔══════════════════════════════════════════════╗"
echo "║         Embedded Linux System Info           ║"
echo "╚══════════════════════════════════════════════╝"

# ── Kernel / OS ────────────────────────────────────────────
echo ""
echo "── Kernel & OS ─────────────────────────────────"
echo "Hostname    : $(hostname)"
echo "Kernel      : $(uname -r)"
echo "Architecture: $(uname -m)"
echo "Build       : $(uname -v)"
echo "OS Release  : $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'=' -f2 | tr -d '"')"
echo "Uptime      : $(uptime -p 2>/dev/null || uptime)"

# ── CPU ────────────────────────────────────────────────────
echo ""
echo "── CPU ─────────────────────────────────────────"
echo "Model       : $(grep 'model name' /proc/cpuinfo | head -1 | awk -F: '{print $2}' | xargs)"
echo "Cores       : $(nproc)"
echo "Max MHz     : $(grep 'cpu MHz' /proc/cpuinfo | awk '{printf "%.0f", $4}' | sort -n | tail -1) MHz"
echo "CPU Flags   : $(grep '^flags' /proc/cpuinfo | head -1 | awk -F: '{print $2}' | tr ' ' '\n' | grep -E 'fpu|vfp|neon|sse' | xargs)"
echo "Load avg    : $(cat /proc/loadavg)"

# ── Memory ─────────────────────────────────────────────────
echo ""
echo "── Memory ──────────────────────────────────────"
free -h
echo ""
echo "Top memory consumers (RSS):"
ps aux --sort=-%mem | head -6 | awk '{printf "  %-10s %5s %5s %s\n", $1, $3, $4, $11}'

# ── Storage ────────────────────────────────────────────────
echo ""
echo "── Storage ─────────────────────────────────────"
df -h | grep -v tmpfs | head -10
echo ""
echo "Block devices:"
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT 2>/dev/null | head -10

# ── Network ────────────────────────────────────────────────
echo ""
echo "── Network Interfaces ──────────────────────────"
ip -brief addr show 2>/dev/null || ifconfig 2>/dev/null | grep -E 'inet|^[a-z]'
echo ""
echo "Default route:"
ip route show default 2>/dev/null || route -n 2>/dev/null | head -4

# ── Processes ──────────────────────────────────────────────
echo ""
echo "── Top CPU Processes ───────────────────────────"
ps aux --sort=-%cpu | head -6 | awk '{printf "  %-10s %5s %5s %s\n", $1, $2, $3, $11}'

# ── GPIO / I2C / SPI (embedded board specific) ─────────────
echo ""
echo "── Hardware Bus Detection ──────────────────────"
ls /dev/i2c-*   2>/dev/null && echo "I2C :  $(ls /dev/i2c-*)   " || echo "I2C :  none"
ls /dev/spidev* 2>/dev/null && echo "SPI :  $(ls /dev/spidev*) " || echo "SPI :  none"
ls /dev/tty*    2>/dev/null | head -5 && echo "UART:  see above"

# ── Kernel messages ────────────────────────────────────────
echo ""
echo "── Last 5 kernel messages ──────────────────────"
dmesg | tail -5 2>/dev/null

echo ""
echo "═══════════════════════════════════════════════"
