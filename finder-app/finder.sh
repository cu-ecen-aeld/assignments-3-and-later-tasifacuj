#!/bin/sh

nargs=$#


if [[ $nargs -ne 2 ]]; then
    echo "missing arguments"
    exit 1
fi

DIR=$1
PAT=$2

if [ ! -d "$DIR" ];then
    echo "$DIR not a directory"
    exit 1
fi

Y=$(grep -r "$PAT" $DIR | awk -F ':' '{print $1}' | wc -l)
X=$(grep -r "$PAT" $DIR | awk -F ':' '{print $1}' | uniq | wc -l)
echo "The number of files are $X and the number of matching lines are $Y"
