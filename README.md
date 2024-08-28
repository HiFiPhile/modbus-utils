modbus-utils
============

Modbus client and server command line tools based on libmodbus.

NOTE:
Both apps are linked with libmodbus library. After repository is pulled do the following:

compilation
===========

## option 1 (make)

```sh
git clone https://github.com/HiFiPhile/modbus-utils
cd modbus-utils

# build libmodbus
pushd ./libmodbus
./configure --enable-static
make
popd

make
```

usage
=====

Run apps with no arguments, descriptive help information will be provided.
