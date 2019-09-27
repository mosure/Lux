#!/bin/bash

readonly DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

npm install --prefix ${DIR}/../api
npm run start --prefix ${DIR}/../api
