#!/bin/sh
cd "$(dirname "$0")"
qdbuscpp2xml -A powerapi.h -o interfaces/powerapi.xml
qdbuscpp2xml -A dbusservice.h -o interfaces/dbusservice.xml
