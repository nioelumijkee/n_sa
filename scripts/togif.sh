#!/bin/bash
# convert tga to gif with delay
# req.:  imagemagick

usage="usage: <dir>(with tga) <delay>(2 normal)"

# dir
if  ! [ -d "$1" ]; then
    echo "error: bad directory"
    echo $usage
    exit 1
fi

# delay = N/100 (sec)
re='^[0-9]+$'
if ! [[ $2 =~ $re ]] ; then
    echo "error: not a number"
    echo $usage
    exit 1
fi 

convert -depth 24 -delay $2 $1/*.tga image.gif
