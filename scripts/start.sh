#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly TASK_NAME="Lux.o"

exec -a $TASK_NAME ${DIR}/../src/Lux.o
