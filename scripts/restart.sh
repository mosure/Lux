#!/bin/bash

readonly TASK_NAME="Lux"

# Kill the current task
if pgrep "$TASK_NAME"; then pkill -f "$TASK_NAME"; fi

# Start it up again
bash -c "exec -a $TASK_NAME ./start.sh"
