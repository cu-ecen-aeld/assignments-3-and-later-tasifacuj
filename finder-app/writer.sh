#!/bin/bash
set -euo

nargs=$#


if [[ $nargs -ne 2 ]]; then
    echo "missing arguments"
    exit 1
fi

FNAME=$1
DIRNAME=$(dirname $FNAME)
CNT=$2

mkdir -p $DIRNAME || {
    echo "Failed to create $DIRNAME"
    exit 1
}

echo $CNT > "$FNAME" || {
    echo "cannot create $FNAME"
    exit 1
}