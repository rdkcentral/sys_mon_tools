#!/bin/bash
set -x
set -e

WORKDIR=`pwd`
export ROOT=/usr
export INSTALL_DIR=${ROOT}/local
mkdir -p $INSTALL_DIR

export IARMBUS_PATH=$ROOT/iarmbus
export IARMMGRS_PATH=$ROOT/iarmmgrs

function standaloneBuildClean()
{
    pd=`pwd`
    CLEAN_BUILD=1
    dnames="$RDK_SCRIPTS_PATH"
    for dName in $dnames
    do
        cd $dName
        if [ -f Makefile ]; then
                make distclean
            fi
        rm -f configure
        rm -rf aclocal.m4 autom4te.cache config.log config.status libtool
            rm -rf install
        find . -iname "Makefile.in" -exec rm -f {} \;
        find . -iname "Makefile" | xargs rm -f
        ls cfg/* | grep -v "Makefile.am" | xargs rm -f
        cd $pd
    done
    find . -iname "*.o" -exec rm -f {} \;
}

function standaloneBuildConfigure()
{
    aclocal -I cfg
    libtoolize --automake
    autoheader
    automake --foreign --add-missing
    rm -f configure
    autoconf
    ./configure
}

echo "##### Building sys_mon_tools module"
cd $WORKDIR

export GLIBS='-lglib-2.0 -lz'
export ROOT_INC="/usr/lib/x86_64-linux-gnu"
export GLIB_INCLUDE_PATH="/usr/include/glib-2.0"
export GLIB_CONFIG_INCLUDE_PATH="${ROOT_INC}/glib-2.0/include"
export DBUS_INCLUDE_PATH="/usr/include/dbus-1.0"
export DBUS_CONFIG_INCLUDE_PATH="${ROOT_INC}/dbus-1.0/include"
export CFLAGS+="-O2 -Wall -fPIC -I./include -I${GLIB_INCLUDE_PATH} -I${GLIB_CONFIG_INCLUDE_PATH} \
	-I/usr/include \
	-I/usr/local/include \
	-I${WORKDIR}/stubs \
	-I${DBUS_INCLUDE_PATH} \
	-I${DBUS_CONFIG_INCLUDE_PATH} \
	-I/usr/include/libsoup-2.4 \
	-I/usr/include/gssdp-1.0"
export LDFLAGS+="-Wl,-rpath, -L/usr/lib"
export CC="gcc $CFLAGS"
export CXX="g++ $CFLAGS $LDFLAGS"
export USE_IARM_BUS="y"

standaloneBuildClean
standaloneBuildConfigure

#make
#
#cp $IARMMGRS_PATH/hal/include/comcastIrKeyCodes.h /usr/local/include
#cp $IARMMGRS_PATH/mfr/include/mfr*.h /usr/local/include
#cp $IARMMGRS_PATH/mfr/common/include/mfrApi.h /usr/local/include

make VERBOSE=1 AM_CPPFLAGS="-I${WORKDIR}/stubs -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/libsoup-3.0 -I/usr/local/include -I${IARMMGRS_PATH}/sysmgr/include -I${IARMBUS_PATH}/core -I${IARMBUS_PATH}/core/include -I${IARMMGRS_PATH}/hal/include -I${IARMMGRS_PATH}/mfr/include -I${IARMMGRS_PATH}/mfr/common/include" \
AM_LDFLAGS="-L/usr/local/lib -lIARMBus -lWPEFrameworkPowerController"  CXXFLAGS="-fpermissive -I${WORKDIR}/stubs"