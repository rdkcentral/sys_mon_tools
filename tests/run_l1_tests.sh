#!/bin/bash
set -ex

WORKDIR="$(pwd)"
ROOT=/usr
IARMBUS_PATH="$ROOT/iarmbus"
IARMMGRS_PATH="$ROOT/iarmmgrs"

export CFLAGS="-O2 -Wall -fPIC --coverage -I./include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include -I/usr/local/include -I${WORKDIR}/stubs -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/libsoup-2.4 -I/usr/include/gssdp-1.0"
export CC=gcc

BUILD_AM_CPPFLAGS="-I${WORKDIR}/stubs -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/libsoup-3.0 -I/usr/local/include -I${IARMMGRS_PATH}/sysmgr/include -I${IARMBUS_PATH}/core -I${IARMBUS_PATH}/core/include -I${IARMMGRS_PATH}/hal/include -I${IARMMGRS_PATH}/mfr/include -I${IARMMGRS_PATH}/mfr/common/include -I${ROOT}/devicesettings/rpc/include -I${ROOT}/rdk-halif-device_settings/include"

GLIB_CFLAGS="$(pkg-config --cflags glib-2.0)"
GLIB_LIBS="$(pkg-config --libs glib-2.0)"

build_and_run()
{
	test_name="$1"
	test_source="$2"
	test_cflags="$3"
	test_libs="$4"
	${CC} ${CFLAGS} ${BUILD_AM_CPPFLAGS} ${test_cflags} -o "${WORKDIR}/tests/${test_name}" "${WORKDIR}/tests/${test_source}" ${test_libs}
	"${WORKDIR}/tests/${test_name}"
}

build_and_run l1_uinput_tests test_uinput_main.c "-DUINPUT_VERSION=5 -I${WORKDIR}/key_simulator" ""
build_and_run test_set_power_state test_set_power_state.c "" "-lpthread"
build_and_run test_query_power_state test_query_power_state.c "" ""
build_and_run test_mfr_utils test_mfr_utils.c "" ""
build_and_run test_key_simulator test_key_simulator.c "-I${WORKDIR}/key_simulator" ""
build_and_run test_iarm_event_sender test_iarm_event_sender.c "${GLIB_CFLAGS}" "${GLIB_LIBS}"
build_and_run test_power_state_monitor test_power_state_monitor.c "${GLIB_CFLAGS}" "${GLIB_LIBS} -lpthread"
build_and_run test_legacy_key_simulator test_legacy_key_simulator.c "-I${WORKDIR}/key_simulator -I${WORKDIR}/tests/mocks" ""