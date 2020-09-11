#!/bin/sh
cd "$(dirname "$0")"
mkdir -p ../../interfaces
qdbuscpp2xml -A dbusservice.h -o ../../interfaces/dbusservice.xml
\ls *api.h | while read file; do
    qdbuscpp2xml -A $file -o ../../interfaces/$(basename $file .h).xml
done
