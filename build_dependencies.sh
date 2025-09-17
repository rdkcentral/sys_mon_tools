#!/bin/bash
set -x
set -e

WORKDIR=`pwd`
export ROOT=/usr
export INSTALL_DIR=${ROOT}/local
mkdir -p $INSTALL_DIR

cd $ROOT
rm -rf iarmbus
git clone https://github.com/rdkcentral/iarmbus.git

cd $ROOT
rm -rf iarmmgrs
git clone https://github.com/rdkcentral/iarmmgrs.git

export IARMBUS_PATH=$ROOT/iarmbus
export IARMMGRS_PATH=$ROOT/iarmmgrs


cd $ROOT
rm -rf devicesettings
git clone https://github.com/rdkcentral/devicesettings.git

# Build and deploy stubs for IARMBus
echo "Building IARMBus stubs"
cd $WORKDIR
cd ./stubs
g++ -fPIC -shared -o libIARMBus.so iarm_stubs.cpp -I$WORKDIR/stubs -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I$IARMBUS_PATH/core -I$IARMBUS_PATH/core/include -I$ROOT/devicesettings/rpc/include -fpermissive
g++ -fPIC -shared -o libWPEFrameworkPowerController.so powerctrl_stubs.cpp  -I$WORKDIR/stubs -fpermissive


cp libIARMBus.so /usr/local/lib
cp libWPEFrameworkPowerController.so /usr/local/lib/libWPEFrameworkPowerController.so
