#!/bin/sh
APP_NAME=$(basename `find build -type f -name *.elf`)
arm-none-eabi-gdb build/$APP_NAME -ex 'target extended-remote :3333'

