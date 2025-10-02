# Build Guide

## You need atleast two terminals for open OpenOCD & programing script.
Setup for environment first. (Need to setup if using new terminal)


## Each terminal should setup environment.
```
# cd framework
# ./tools/setupscripts/ubuntu-setup.sh
# . ./tools/setupscripts/env.sh
```



## Run OpenOCD for GDB service.
New Terminal, remember to do environment setup.
```
# cd framework
# openocd -f src/platforms/<your platform>/openocd.cfg
ex. openocd -f src/platforms/awh575-mf1-v2/openocd.cfg
```


## Program configuration sample file to awh575-mf1-v2.
New Terminal, remember to do environment setup. Edit personal configuration if you need.
```
# cd framework
# ./bcf_write <your platform> <module counrty>
ex. ./bcf_write awh575-mf1-v2 US

// Use "pipenv sync" if you need.
```


## Build ping sample code for platform.
```
# cd examples/<target application>/targets/<your platform>
ex. cd examples/ping/targets/awh575-mf1-v2

# make clean && make -j$(nproc)
```


## Run GDB script for burn image to MCU build-in flash
```
# ./burn_run.sh
```
**Or run GDB manually.**
```
# ./flash
# monitor reset halt
# load
# c
```
