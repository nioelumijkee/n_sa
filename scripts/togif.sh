#!/bin/bash
# convert tga to gif with delay


usage="USAGE: DIR(with tga) DELAY(2 normal)"

# DIR
if  ! [ -d "$1" ]; then
    echo "error: bad directory"
    echo $usage
    exit 1
fi

# DELAY = N/100 (sec)
re='^[0-9]+$'
if ! [[ $2 =~ $re ]] ; then
    echo "error: not a number"
    echo $usage
    exit 1
fi 

convert -depth 24 -delay $2 $1/*.tga image.gif
