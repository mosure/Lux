#!/bin/bash

readonly TASK_NAME="Lux"

# Kill the current task
pkill -f "$TASK_NAME"

# Reset to origin master
git fetch --all

git reset --hard origin/master

# Build the new C Application from Source
./build.sh

bash -c "exec -a $TASK_NAME ./start.sh"
