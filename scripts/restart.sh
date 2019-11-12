#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly TASK_NAME="Lux.o"

# Kill the current task
if pgrep "$TASK_NAME"; then pkill -f "$TASK_NAME"; fi

# Start it up again
bash -c "${DIR}/start.sh" &
