#!/bin/sh
cd "$(dirname "$0")"
mkdir -p ../../interfaces
p(){
  echo "qdbuscpp2xml $1"
  qdbuscpp2xml -A "$1" -o ../../interfaces/"$2".xml
}

p dbusinterface.h blight
