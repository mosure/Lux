#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly TASK_NAME="Lux"

# Kill the current task
if pgrep "$TASK_NAME"; then pkill -f "$TASK_NAME"; fi

# Reset to origin master
git fetch --all

git reset --hard origin/master

# Build the new C Application from Source
"${DIR}/build.sh"

bash -c "exec -a $TASK_NAME ${DIR}/start.sh"
