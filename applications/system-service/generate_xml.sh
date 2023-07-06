#!/bin/sh
cd "$(dirname "$0")"
mkdir -p ../../interfaces
qdbuscpp2xml=/usr/bin/qdbuscpp2xml
if [ ! -f $qdbuscpp2xml ];then
  qdbuscpp2xml=qdbuscpp2xml
fi
p(){
  echo "qdbuscpp2xml $1"
  $qdbuscpp2xml -A "$1" -o ../../interfaces/"$(basename "$1" .h)".xml
}

p dbusservice.h
p network.h
p bss.h
p application.h
p screenshot.h
p notification.h
p window.h
\ls ./*api.h | while read file; do
    p "$file"
done
