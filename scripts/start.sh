#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
readonly TASK_NAME="Lux"

exec -a $TASK_NAME ${DIR}/../src/main.o
