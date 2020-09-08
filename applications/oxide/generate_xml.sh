#!/bin/sh
cd "$(dirname "$0")"
qdbuscpp2xml -A batteryapi.h -o interfaces/batteryapi.xml
qdbuscpp2xml -A dbusservice.h -o interfaces/dbusservice.xml
