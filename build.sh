#!/bin/sh
#
# build.sh - build the giulia firmware image.
#
# The EDK2 python build needs a working /dev/shm for its multiprocessing
# queues.  On this system that directory keeps disappearing, which makes the
# build fail with "FileNotFoundError" deep inside multiprocessing.  Recreate
# it before every build so the failure never wastes a run.
#
# Usage:  ./build.sh [extra build_uefi.py args...]
#
set -e
cd "$(dirname "$0")"

if [ ! -d /dev/shm ]; then
    mkdir -p /dev/shm 2>/dev/null || {
        echo '114514' | sudo -S mkdir -p /dev/shm 2>/dev/null
        echo '114514' | sudo -S chmod 1777 /dev/shm 2>/dev/null
    }
fi

# Quick sanity check that multiprocessing works before the long build.
python3 -c "import multiprocessing as m; m.Queue(4)" 2>/dev/null || {
    echo "error: /dev/shm is not usable, semaphores cannot be created" >&2
    exit 1
}

exec python3 build_uefi.py -d giulia -r DEBUG "$@"
