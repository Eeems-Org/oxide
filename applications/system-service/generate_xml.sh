#!/bin/sh
cd "$(dirname "$0")"
mkdir -p ../../interfaces
qdbuscpp2xml -A dbusservice.h -o ../../interfaces/dbusservice.xml
qdbuscpp2xml -A network.h -o ../../interfaces/network.xml
qdbuscpp2xml -A bss.h -o ../../interfaces/bss.xml
\ls *api.h | while read file; do
    qdbuscpp2xml -A $file -o ../../interfaces/$(basename $file .h).xml
done
