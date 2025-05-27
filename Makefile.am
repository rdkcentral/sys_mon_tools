##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
SUBDIRS =
DIST_SUBDIRS =

# the list of header files that belong to the library (to be installed later)
AM_CFLAGS = \
	$(DBUS_CFLAGS) $(GLIB_CFLAGS) \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs-hal \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/ir \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/mfr \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/power \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/rdmmgr \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/receiver \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmmgrs/sysmgr \
	-I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/WPEFramework/powercontroller \
	-L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib/


include_HEADERS = $(top_srcdir)/key_simulator/RDKIrKeyCodes.h

bin_PROGRAMS = keySimulator mfr_util QueryPowerState SetPowerState IARM_event_sender

mfr_util_SOURCES = mfr-utils/sys_mfr_utils.c

QueryPowerState_SOURCES=iarm_query_powerstate/IARM_Bus_CheckPowerStatus.c
QueryPowerState_LDADD = $(DIRECT_LIBS) $(FUSION_LIBS) $(GLIB_LIBS) $(DBUS_LIBS)

SetPowerState_SOURCES=iarm_set_powerstate/IARM_BUS_SetPowerStatus.c
SetPowerState_LDADD = -ldbus-1 -lstdc++ -lpthread

keySimulator_SOURCES=key_simulator/IARM_BUS_UIEventSimulator.c key_simulator/uinput.c
keySimulator_LDADD = $(GLIB_LIBS) $(DBUS_LIBS)

IARM_event_sender_SOURCES = iarm-event-sender/IARM_event_sender.c
IARM_event_sender_LDADD = $(DIRECT_LIBS) $(FUSION_LIBS) $(GLIB_LIBS) $(DBUS_LIBS)

if STANDALONE_BUILD
AM_CFLAGS += -I$(top_srcdir)/mocks -DSTANDALONE_BUILD
include_HEADERS += $(top_srcdir)/mocks/IrKeyCodes.h
mfr_util_SOURCES += $(top_srcdir)/mocks/mockInternal.cpp
QueryPowerState_SOURCES += $(top_srcdir)/mocks/mockInternal.cpp
SetPowerState_SOURCES += $(top_srcdir)/mocks/mockInternal.cpp
keySimulator_SOURCES += $(top_srcdir)/mocks/mockInternal.cpp
IARM_event_sender_SOURCES += $(top_srcdir)/mocks/mockInternal.cpp
else
mfr_util_LDADD = -lIARMBus
keySimulator_LDADD += -lIARMBus
QueryPowerState_LDADD += -lWPEFrameworkPowerController
SetPowerState_LDADD += -lWPEFrameworkPowerController
IARM_event_sender_LDADD += -lIARMBus
endif
