#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Reset to origin master
git pull

# Build the new C Application from Source
"${DIR}/build.sh"

"${DIR}/restart.sh"
