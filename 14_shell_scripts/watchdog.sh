#!/bin/bash
# 14_shell_scripts/watchdog.sh
# ============================
# Simple software watchdog: monitors a process and restarts it if it dies.
# Real embedded systems use hardware WDT (/dev/watchdog) but software
# watchdogs are common in userspace daemons.
#
# Usage: ./watchdog.sh <process_name> <command_to_restart>
# Example: ./watchdog.sh my_daemon "/usr/bin/my_daemon --config /etc/my_daemon.conf"
#
# Interview topics:
#   Q: What is a hardware watchdog?
#   A: A hardware timer that resets the CPU if not "kicked" periodically.
#      Driver: /dev/watchdog. Write any byte to reset timer, close to stop.
#      ioctl(WDIOC_KEEPALIVE) — recommended API.
#
#   Q: Difference between software and hardware watchdog?
#   A: HW WDT persists even if the OS/kernel hangs.
#      SW watchdog is only useful for process-level failures.

set -euo pipefail

PROCESS_NAME="${1:-}"
RESTART_CMD="${2:-}"
CHECK_INTERVAL=5   # seconds between checks
MAX_RESTARTS=5     # give up after this many restarts
LOG_FILE="/tmp/watchdog_${PROCESS_NAME}.log"

if [[ -z "$PROCESS_NAME" || -z "$RESTART_CMD" ]]; then
    echo "Usage: $0 <process_name> <restart_command>"
    exit 1
fi

restart_count=0
echo "[$(date '+%F %T')] Watchdog started for: $PROCESS_NAME" | tee -a "$LOG_FILE"
echo "[$(date '+%F %T')] Restart command: $RESTART_CMD"       | tee -a "$LOG_FILE"

while true; do
    if pgrep -x "$PROCESS_NAME" > /dev/null 2>&1; then
        # Process is alive — optionally kick HW watchdog here:
        # echo 1 > /dev/watchdog
        :
    else
        restart_count=$((restart_count + 1))
        echo "[$(date '+%F %T')] WARN: '$PROCESS_NAME' not running! Restart #$restart_count" | tee -a "$LOG_FILE"

        if (( restart_count > MAX_RESTARTS )); then
            echo "[$(date '+%F %T')] ERROR: Max restarts ($MAX_RESTARTS) exceeded. Giving up." | tee -a "$LOG_FILE"
            exit 1
        fi

        # Restart the process in background
        eval "$RESTART_CMD" &
        echo "[$(date '+%F %T')] Started '$PROCESS_NAME' (PID $!)" | tee -a "$LOG_FILE"
    fi

    sleep "$CHECK_INTERVAL"
done
