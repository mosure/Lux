#!/bin/bash

readonly TASK_NAME="Lux"

# Kill the current task
pkill -f "$TASK_NAME"

# Start it up again
bash -c "exec -a $TASK_NAME ./start.sh"
