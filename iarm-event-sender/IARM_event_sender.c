/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "sysMgr.h"
#ifdef HAS_MAINTENANCE_MANAGER
#include "maintenanceMGR.h"
#endif
#ifdef PLATFORM_SUPPORTS_RDMMGR
#include "rdmMgr.h"
#endif
#include "libIARMCore.h"
#include "libIARM.h"
#include <glib.h>
#ifdef CTRLM_ENABLED
#include "ctrlm_ipc_device_update.h"
#endif
#ifdef HAS_WIFI_SUPPORT
#include "netsrvmgrIarm.h"
#endif
#include <stdbool.h>
#include "dsMgr.h"

#define MAX_ARG_NO_TYPE        (6)

/*
 * -----------------------------
 * Argument Assignment Macros
 * -----------------------------
 */
#define INT_ARG(n)   args[n].val.i
#define BOOL_ARG(n)  args[n].val.b
#define STR_ARG(n)   args[n].val.s


/*
 * -----------------------------
 * Argument Types
 * -----------------------------
 */
typedef enum {
    ARG_INT,
    ARG_STRING,
    ARG_BOOL
} ArgType;

typedef struct {
    ArgType type;
    union {
        int   i;
        bool  b;
        const char *s;
    } val;
} ArgValue;

/*
 * -----------------------------
 * Event Handling
 * -----------------------------
 */
typedef void (*IARM_EventHandler)(ArgValue *args);

typedef struct {
    const char *name;
    int argc_expected;
    ArgType arg_types[MAX_ARG_NO_TYPE];
    IARM_EventHandler handler;
} IARM_EventEntry;

static void printUsage(const char *prog);
static bool parseArg(const char *arg, ArgType type, ArgValue *out);
static void handleIARMEvents(int argc, char *argv[]);

static void handleHdmiAllmEvent(ArgValue *args);
static void handleHdmiVrrEvent(ArgValue *args);
static void handleHdmiInSignalStatus(ArgValue *args);
static void handleHdmiInHotPlug(ArgValue *args);
static void handleHdmiInStatus(ArgValue *args);
static void handleHdmiInVideoModeUpdate(ArgValue *args);
static void handleCompositeInVideoModeUpdate(ArgValue *args);
static void handleCompositeInStatus(ArgValue *args);
static void handleCompositeInSignalStatus(ArgValue *args);
static void handleCompositeInHotPlug(ArgValue *args);
static void handleHdmiInAviContentType(ArgValue *args);
static void handleAudioOutHotPlug(ArgValue *args);
static void handleAudioFormatUpdate(ArgValue *args);
static void handleAudioSecondaryLanguageChanged(ArgValue *args);
static void handleAudioPrimaryLanguageChanged(ArgValue *args);
static void handleAudioFaderControl(ArgValue *args);
static void handleAudioPortState(ArgValue *args);
static void handleAudioMode(ArgValue *args);
static void handleDisplayFrameRatePreChange(ArgValue *args);
static void handleDisplayFrameRatePostChange(ArgValue *args);
static void handleAtmosCapsChanged(ArgValue *args);
static void handleEventRxSense(ArgValue *args);
static void handleHdmiHotPlug(ArgValue *args);
static void handleAudioLevelChanged(ArgValue *args);
static void handleVideoFormatUpdate(ArgValue *args);
static void handleDisplayResolutionPreChange(ArgValue *args);
static void handleDisplayResolutionPostChange(ArgValue *args);
static void handleHdmiInAvLatency(ArgValue *args);
static void handleAudioMixingChanged(ArgValue *args);
static void handleEventZoomSetting(ArgValue *args);
static void handleEventHdcpStatus(ArgValue *args);

IARM_Result_t sendIARMEvent(GString* currentEventName, unsigned char eventStatus);
IARM_Result_t sendIARMEventPayload(GString* currentEventName, char *eventPayload);
IARM_Result_t sendCustomIARMEvent(int stateId, int state, int error);
static struct eventList{
	gchar* eventName;
	unsigned char sysStateEvent;
}eventList[]=
{
	{"ImageDwldEvent",IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_DWNLD},
	{"GatewayConnEvent",IARM_BUS_SYSMGR_SYSSTATE_GATEWAY_CONNECTION},
	{"TuneReadyEvent",IARM_BUS_SYSMGR_SYSSTATE_TUNEREADY},
	{"MocaStatusEvent",IARM_BUS_SYSMGR_SYSSTATE_MOCA},
	{"ChannelMapEvent",IARM_BUS_SYSMGR_SYSSTATE_CHANNELMAP},
	{"NTPReceivedEvent",IARM_BUS_SYSMGR_SYSSTATE_TIME_SOURCE},
	{"PartnerIdEvent",IARM_BUS_SYSMGR_SYSSTATE_PARTNERID_CHANGE},
    {"FirmwareStateEvent",IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE},
    {"IpmodeEvent",IARM_BUS_SYSMGR_SYSSTATE_IP_MODE},
    {"usbdetected",IARM_BUS_SYSMGR_SYSSTATE_USB_DETECTED},
    {"LogUploadEvent",IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD},
    {"RedStateEvent",IARM_BUS_SYSMGR_SYSSTATE_RED_RECOV_UPDATE_STATE}
};

/* Table only used for IARM Events */

static IARM_EventEntry iarmEventTable[] = {
    { "DSMgr_CompositeInHotPlug",            2, { ARG_INT, ARG_BOOL }, handleCompositeInHotPlug },
    { "DSMgr_CompositeInSignalStatus",       2, { ARG_INT, ARG_INT  }, handleCompositeInSignalStatus },
    { "DSMgr_CompositeInStatus",             2, { ARG_INT, ARG_BOOL }, handleCompositeInStatus },
    { "DSMgr_CompositeInVideoModeUpdate",    4, { ARG_INT, ARG_INT, ARG_INT, ARG_INT }, handleCompositeInVideoModeUpdate },
    { "DSMgr_HdmiInVideoModeUpdate",         4, { ARG_INT, ARG_INT, ARG_INT, ARG_INT }, handleHdmiInVideoModeUpdate },
    { "DSMgr_HdmiAllmEvent",                 2, { ARG_INT, ARG_INT  }, handleHdmiAllmEvent },
    { "DSMgr_HdmiVrrEvent",                  2, { ARG_INT, ARG_INT  }, handleHdmiVrrEvent },
    { "DSMgr_HdmiInStatus",                  2, { ARG_INT, ARG_BOOL }, handleHdmiInStatus },
    { "DSMgr_HdmiInSignalStatus",            2, { ARG_INT, ARG_INT  }, handleHdmiInSignalStatus },
    { "DSMgr_HdmiInHotPlug",                 2, { ARG_INT, ARG_BOOL }, handleHdmiInHotPlug },
    { "DSMgr_HdmiInAviContentType",          2, { ARG_INT, ARG_INT  }, handleHdmiInAviContentType },
    { "DSMgr_AudioOutHotPlug",               3, { ARG_INT, ARG_INT, ARG_BOOL}, handleAudioOutHotPlug },
    { "DSMgr_AudioFormatUpdate",             1, { ARG_INT}, handleAudioFormatUpdate },
    { "DSMgr_AudioSecondaryLanguageChanged", 1, { ARG_STRING}, handleAudioSecondaryLanguageChanged },
    { "DSMgr_AudioPrimaryLanguageChanged",   1, { ARG_STRING}, handleAudioPrimaryLanguageChanged },
    { "DSMgr_AudioFaderControl",             1, { ARG_INT}, handleAudioFaderControl },
    { "DSMgr_AudioMixingChanged",            1, { ARG_INT}, handleAudioMixingChanged },
    { "DSMgr_AudioPortState",                1, { ARG_INT}, handleAudioPortState },
    { "DSMgr_AudioMode",                     2, { ARG_INT, ARG_INT  }, handleAudioMode },
    { "DSMgr_DisplayFrameRatePreChange",     1, { ARG_STRING}, handleDisplayFrameRatePreChange },
    { "DSMgr_DisplayFrameRatePostChange",    1, { ARG_STRING}, handleDisplayFrameRatePostChange },
    { "DSMgr_AtmosCapsChanged",              2, { ARG_INT, ARG_BOOL}, handleAtmosCapsChanged },
    { "DSMgr_EventRxSense",                  1, { ARG_INT }, handleEventRxSense },
    { "DSMgr_EventZoomSettings",             1, { ARG_INT }, handleEventZoomSetting },
    { "DSMgr_HdmiHotPlug",                   1, { ARG_BOOL }, handleHdmiHotPlug },
    { "DSMgr_AudioLevelChanged",             1, { ARG_INT }, handleAudioLevelChanged },
    { "DSMgr_VideoFormatUpdate",             1, { ARG_INT }, handleVideoFormatUpdate },
    { "DSMgr_DisplayResolutionPreChange",    2, { ARG_INT, ARG_INT }, handleDisplayResolutionPreChange },
    { "DSMgr_DisplayResolutionPostChange",   2, { ARG_INT, ARG_INT }, handleDisplayResolutionPostChange },
    { "DSMgr_HdmiInAvLatency",               2, { ARG_INT, ARG_INT }, handleHdmiInAvLatency },
    { "DSMgr_EventHdcpStatus",               1, { ARG_STRING }, handleEventHdcpStatus },
};
static const int iarmEventTableSize = sizeof(iarmEventTable) / sizeof(iarmEventTable[0]);



/*

Usage Applicable only for IARM Events:
 
IARM_event_sender DSMgr_CompositeInHotPlug <port> <true/false>
IARM_event_sender DSMgr_CompositeInSignalStatus <port> <signalvalue>
IARM_event_sender DSMgr_CompositeInStatus <port> <true/false>
IARM_event_sender DSMgr_CompositeInVideoModeUpdate <port> <pixelresolution> <interlaced> <frameRate>
IARM_event_sender DSMgr_HdmiAllmEvent <port> <allm_mode>
IARM_event_sender DSMgr_HdmiInVideoModeUpdate <port> <pixelresolution> <interlaced> <frameRate>
IARM_event_sender DSMgr_HdmiVrrEvent <port> <vrrvalue>
IARM_event_sender DSMgr_HdmiInStatus <port> <ispresented>
IARM_event_sender DSMgr_HdmiInSignalStatus <port> <signalvalue>
IARM_event_sender DSMgr_HdmiInHotPlug <port> <true/false>
IARM_event_sender DSMgr_HdmiInAviContentType <port> <avi_content_type>
IARM_event_sender DSMgr_AudioOutHotPlug <port_type> <port> <isconnected:true/false>
IARM_event_sender DSMgr_AudioFormatUpdate <Audio_format>
IARM_event_sender DSMgr_AudioPrimaryLanguageChanged <lang string>
IARM_event_sender DSMgr_AudioSecondaryLanguageChanged <lang string>
IARM_event_sender DSMgr_AudioFaderControl <Audio_faderval>
IARM_event_sender DSMgr_AudioMixingChanged <Audio_mixingval>
IARM_event_sender DSMgr_AudioPortState <Audio_PortStateVal>
IARM_event_sender DSMgr_AudioMode <porttype> <audiomode>
IARM_event_sender DSMgr_DisplayFrameRatePreChange <framerate_string>
IARM_event_sender DSMgr_DisplayFrameRatePostChange <framerate_string>
IARM_event_sender DSMgr_AtmosCapsChanged <atmosCaps> <true/false>
IARM_event_sender DSMgr_EventRxSense <rxsense_value>
IARM_event_sender DSMgr_EventZoomSettings <zoom_value>
IARM_event_sender DSMgr_HdmiHotPlug <true/false>
IARM_event_sender DSMgr_AudioLevelChanged <audio_value>
IARM_event_sender DSMgr_VideoFormatUpdate <video_format_value_in_binary_bit_position>
IARM_event_sender DSMgr_HdmiInAvLatency <audio_output_delay> <video_latency>
IARM_event_sender handleEventHdcpStatus <string_none>
 
*/
 

#define EVENT_INTRUSION "IntrusionEvent"
#define INTRU_ABREV '+' // last character for abreviated buffer
#define JSON_TERM "\"}]}" // valid termination for overflowed buffer

int main(int argc,char *argv[])
{
    g_message("IARM_event_sender  Entering %d\r\n", getpid());
    GString *currentEventName=g_string_new(NULL);

    if (argc == 3)
    {
        unsigned char eventStatus;
        g_string_assign(currentEventName,argv[1]);
        eventStatus=atoi(argv[2]);
        g_message(">>>>> Send IARM_BUS_NAME EVENT current Event Name =%s,evenstatus=%d",currentEventName->str,eventStatus);
        sendIARMEvent(currentEventName,eventStatus);
        return 0;
    }
    else if (argc >= 1 && !strncmp(argv[1], "DSMgr_", 5)) {
        handleIARMEvents(argc, argv);
    } 
    else if (argc == 4)
    {
        char eventPayload[ IARM_BUS_SYSMGR_Intrusion_MaxLen+1 ]; // iarm payload is limited in size. this can check for overflow
	unsigned short full_len;
        g_string_assign(currentEventName,argv[1]);
        g_message(" Send %s",currentEventName->str );

        if ( !(g_ascii_strcasecmp(currentEventName->str,"PeripheralUpgradeEvent")) )
        {
            full_len = snprintf(eventPayload, sizeof(eventPayload), "%s:%s", argv[2], argv[3]);
        }
        else
        {
            full_len=strlen(argv[3]);
            strncpy( eventPayload, argv[3], sizeof(eventPayload) );
	    eventPayload[sizeof(eventPayload)-1] = '\0';
        }

        eventPayload[sizeof(eventPayload)-1]='\0'; // null terminated in case of overflow
        // check if input event status was too long
        if ( strnlen( eventPayload, sizeof(eventPayload) ) < full_len )
        {
            char *termptr;
            g_message(" Send abreviated IARM_BUS_NAME EVENT %s",currentEventName->str );
            // abreviation marker '+' followed by json termination
            termptr = &eventPayload[sizeof(eventPayload)-(sizeof(JSON_TERM)-1)-3];
            *termptr++ = INTRU_ABREV;
            strncpy( termptr, JSON_TERM, sizeof(JSON_TERM)-1 );
	    termptr[sizeof(JSON_TERM)-1] = '\0';
            eventPayload[sizeof(eventPayload)-1]='\0'; // null terminated just in case 
            // go ahead and send abreviated event now that it has valid josn termination
        }

        g_message(">>>>> Send IARM_BUS_NAME EVENT current Event Name =%s,evenstatus=%s",currentEventName->str,eventPayload);
        sendIARMEventPayload(currentEventName,eventPayload);
        return 0;
    }
    else if (argc == 5 && !strcmp("CustomEvent", argv[1]))
    {
        int stateId = atoi(argv[2]);
        int state = atoi(argv[3]);
        int error = atoi(argv[4]);
        
        g_message(">>>>> Send Custom Event stateId:%d state:%d error:%d", stateId, state, error );
        
        sendCustomIARMEvent(stateId, state, error);
        
        return 0;
    }
    else if (argc == 5 && !strcmp("USBMountChangedEvent", argv[1]))
    {
        g_string_assign(currentEventName,argv[1]);

        size_t n = 1024;
        char eventPayload[n];
        snprintf(eventPayload, n, "%s:%s:%s", argv[2], argv[3], argv[4]);
        eventPayload[n - 1] = '\0';

        g_message(">>>>> Send USBMountChangedEvent %s", eventPayload);

        sendIARMEventPayload(currentEventName,eventPayload);

        return 0;
    }
    else if (argc == 6 && !strcmp("usbdetected", argv[1]))
    {
        g_string_assign(currentEventName,argv[1]);

        size_t n = 1024;
        char eventPayload[n];
        memset(eventPayload,'\0',n);
        snprintf(eventPayload, n, "%s:%s:%s:%s", argv[2], argv[3], argv[4],argv[5]);

        g_message(">>>>> Send USBDetected Event %s", eventPayload);

        sendIARMEventPayload(currentEventName,eventPayload);

        return 0;
    }
    else if (argc == 6)
    {
        g_string_assign(currentEventName,argv[1]);
        char eventPayload[ 24+1 ]; //(6 x 4)

	if( !(g_ascii_strcasecmp(currentEventName->str,"EISSAppIdEvent")))
        {
            int i,j,k = 0;
            long long app_value = 0;

            for(i = 0; i < 4; i++)
            {
                app_value = (long long)(atoi(argv[2 + i]));

                for(j = 5; j >= 0; j--)
                {
                    eventPayload[k++] = ((app_value >> (j*8)) & 0x0000000000FF);
                    //g_message("Application id shifted %d times : 0x%x",j ,(app_value >> (j*8)));
                }
            }
        }
        g_message(">>>>> Send IARM_BUS_NAME EVENT current Event Name =%s,evenstatus=%s",currentEventName->str,eventPayload);
        sendIARMEventPayload(currentEventName,eventPayload);
        return 0;
    }
    else
    {
        g_message("-----------------------------------------------------------------\n");
        g_message("Normal Usage: %s <event name > <event status> \n",argv[0]);
        g_message("Custom Usage: %s CustomEvent <event stateId> <event state> <event error> \n",argv[0]);
        g_message("(%d)\n",argc );
        g_message("-----------------------------------------------------------------\n");
        printUsage(argv[0]);
        return 1;
    }
}

IARM_Result_t sendCustomIARMEvent(int stateId, int state, int error)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    gboolean eventMatch = FALSE;
    IARM_Bus_SYSMgr_EventData_t eventData;
    
    IARM_Bus_Init("CustomEvent");
    IARM_Bus_Connect();
    
    eventData.data.systemStates.stateId = stateId;
    eventData.data.systemStates.state = state;
    eventData.data.systemStates.error = error;
    
    retCode = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, (void *)&eventData, sizeof(eventData));
    
    if(retCode == IARM_RESULT_SUCCESS)
        g_message(">>>>> IARM SUCCESS  Event - State Id = %d, Event status = %d", stateId, eventData.data.eissEventData.filterStatus);
    else
        g_message(">>>>> IARM FAILURE  Event - State Id = %d, Event status = %d", stateId, eventData.data.eissEventData.filterStatus);
    
    IARM_Bus_Disconnect();
    IARM_Bus_Term();

    return retCode;
}


IARM_Result_t sendIARMEvent(GString* currentEventName,unsigned char eventStatus)
{
	IARM_Result_t retCode = IARM_RESULT_SUCCESS;
	gboolean eventMatch=FALSE;
	int i;
	IARM_Bus_SYSMgr_EventData_t eventData;
        IARM_Bus_Init(currentEventName->str);
        IARM_Bus_Connect();
        g_message(">>>>> Generate IARM_BUS_NAME EVENT current Event Name =%s,eventstatus=%d",currentEventName->str,eventStatus);
        int len = sizeof(eventList)/sizeof( struct eventList );

        if( !(g_ascii_strcasecmp(currentEventName->str,"EISSFilterEvent")))
        {
             eventData.data.eissEventData.filterStatus = (unsigned int)eventStatus;
             g_message(">>>>> Identified EISSFilterEvent");
             retCode=IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_EISS_FILTER_STATUS, (void *)&eventData, sizeof(eventData));
             if(retCode == IARM_RESULT_SUCCESS)
                g_message(">>>>> IARM SUCCESS  Event - IARM_BUS_SYSMGR_EVENT_EISS_FILTER_STATUS,Event status =%d",eventData.data.eissEventData.filterStatus);
             else
                g_message(">>>>> IARM FAILURE  Event - IARM_BUS_SYSMGR_EVENT_EISS_FILTER_STATUS,Event status =%d",eventData.data.eissEventData.filterStatus);

        }
#ifdef HAS_MAINTENANCE_MANAGER
        if ( !(g_ascii_strcasecmp(currentEventName->str,"MaintenanceMGR")) )
        {
            IARM_Bus_MaintMGR_EventData_t infoStatus;

            memset( &infoStatus, 0, sizeof(IARM_Bus_MaintMGR_EventData_t) );
            g_message(">>>>> Identified MaintenanceMGR");
            infoStatus.data.maintenance_module_status.status = (IARM_Maint_module_status_t)eventStatus;
            retCode=IARM_Bus_BroadcastEvent(IARM_BUS_MAINTENANCE_MGR_NAME,(IARM_EventId_t)IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, (void *)&infoStatus, sizeof(infoStatus));
            g_message(">>>>> IARM %s  Event  = %d",(retCode == IARM_RESULT_SUCCESS) ? "SUCCESS" : "FAILURE",\
                    infoStatus.data.maintenance_module_status.status);
            IARM_Bus_Disconnect();
            IARM_Bus_Term();
            g_message("IARM_event_sender closing \r\n");
            return retCode;
        }
#endif
#ifdef HAS_WIFI_SUPPORT
        if( !(g_ascii_strcasecmp(currentEventName->str,"WiFiInterfaceStateEvent")))
        {
             IARM_BUS_NetSrvMgr_Iface_EventData_t param = {0};
             param.isInterfaceEnabled = eventStatus ? true : false;
             retCode=IARM_Bus_BroadcastEvent(IARM_BUS_NM_SRV_MGR_NAME, (IARM_EventId_t) IARM_BUS_NETWORK_MANAGER_EVENT_WIFI_INTERFACE_STATE, (void *)&param, sizeof(param));
             g_message(">>>>> IARM %s  Event - IARM_BUS_NETWORK_MANAGER_EVENT_WIFI_INTERFACE_STATE, interface enabled = %d",
                 (retCode == IARM_RESULT_SUCCESS) ? "SUCCESS" : "FAILURE", param.isInterfaceEnabled);
        }
#endif // HAS_WIFI_SUPPORT
#ifdef PLATFORM_SUPPORTS_RDMMGR
        if( !(g_ascii_strcasecmp(currentEventName->str,"AppDownloadEvent")))
        {
            g_message(">>>>> Identified App Download status message");
            retCode=IARM_Bus_BroadcastEvent(IARM_BUS_RDMMGR_NAME, (IARM_EventId_t) IARM_BUS_RDMMGR_EVENT_APPDOWNLOADS_CHANGED, (void *)&eventStatus, sizeof(eventStatus));
            if(retCode == IARM_RESULT_SUCCESS)
                g_message(">>>>> IARM SUCCESS  Event - IARM_BUS_SYSMGR_EVENT_APP_DNLD ");
            else
                g_message(">>>>> IARM FAILURE  Event - IARM_BUS_SYSMGR_EVENT_APP_DNLD ");
        }
#endif  
        else
        {
            for ( i=0; i < len; i++ )
            {
	        if( !(g_ascii_strcasecmp(currentEventName->str,eventList[i].eventName)))
                {
		        eventData.data.systemStates.stateId = eventList[i].sysStateEvent;
		        eventData.data.systemStates.state = eventStatus;
			eventMatch=TRUE;
                        break;
                }
            }
	    if(eventMatch == TRUE)
	    {
	        eventData.data.systemStates.error = 0;
	        retCode=IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, (void *)&eventData, sizeof(eventData));
	        if(retCode == IARM_RESULT_SUCCESS)
		        g_message(">>>>> IARM SUCCESS  Event Name =%s,sysStateEvent=%d",eventList[i].eventName,eventList[i].sysStateEvent);
	        else
	        	g_message(">>>>> IARM FAILURE  Event Name =%s,sysStateEvent=%d",eventList[i].eventName,eventList[i].sysStateEvent);
	    }
	    else
	    {
		g_message("There are no matching IARM sys events for %s",currentEventName->str);
	    }
	}
	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	g_message("IARM_event_sender closing \r\n");
	return retCode;
}

IARM_Result_t sendIARMEventPayload(GString* currentEventName, char *eventPayload)
{
	IARM_Result_t retCode = IARM_RESULT_SUCCESS;
	gboolean eventMatch=FALSE;
	int i;
	IARM_Bus_Init(currentEventName->str);
	IARM_Bus_Connect();
	g_message(">>>>> Generate IARM_BUS_NAME EVENT current Event Name =%s,eventpayload=%s",currentEventName->str,eventPayload);

	// first check for intrusion event, if not that then check for sysstate events
	if( !(g_ascii_strcasecmp(currentEventName->str,EVENT_INTRUSION )))
	{
		IARM_Bus_SYSMgr_IntrusionData_t intrusionEvent;
		strncpy(intrusionEvent.intrusionData, eventPayload, sizeof(intrusionEvent.intrusionData)-1);
		intrusionEvent.intrusionData[sizeof(intrusionEvent.intrusionData)-1] = '\0';
		retCode=IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_INTRUSION, 
						(void *)&intrusionEvent, sizeof(intrusionEvent));
		g_message(">>>>> IARM %s  Event Name =%s,payload=%s",
			(retCode == IARM_RESULT_SUCCESS)?"SUCCESS":"FAILURE",
			EVENT_INTRUSION, intrusionEvent.intrusionData );
	}
	else if( !(g_ascii_strcasecmp(currentEventName->str,"EISSAppIdEvent")))
	{
             g_message("IARM_event_sender entered case for EISSAppIdEvent\r\n");
             IARM_Bus_SYSMgr_EventData_t eventData;
             memset(eventData.data.eissAppIDList.idList,0,sizeof(eventData.data.eissAppIDList.idList));
             memcpy(eventData.data.eissAppIDList.idList,eventPayload , sizeof(eventData.data.eissAppIDList.idList));
             eventData.data.eissAppIDList.count = 4;

             /*Printing IARM event*/
             int k,l;
             g_message("IARM data : \n");
             for(k = 0; k < 4; k++ )
             {
                 for(l = 0;l < 6;l++)
                 g_message("0x%x ", eventData.data.eissAppIDList.idList[k][l]);

                 g_message("\n");
             }

             IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_EISS_APP_ID_UPDATE, (void *)&eventData, sizeof(eventData));
	}
        #ifdef CTRLM_ENABLED
        else if( !(g_ascii_strcasecmp(currentEventName->str,"PeripheralUpgradeEvent")))
        {
             g_message("IARM_event_sender entered case for PeripheralUpgradeEvent : %s\r\n",eventPayload);
             ctrlm_device_update_iarm_call_update_available_t firmwareInfo;
             firmwareInfo.api_revision=CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION;
             memset(firmwareInfo.firmwareLocation,0,CTRLM_DEVICE_UPDATE_PATH_LENGTH);
             memset(firmwareInfo.firmwareNames,0,CTRLM_DEVICE_UPDATE_PATH_LENGTH);

             memcpy(firmwareInfo.firmwareLocation, strtok(eventPayload, ":"),CTRLM_DEVICE_UPDATE_PATH_LENGTH);
             memcpy(firmwareInfo.firmwareNames, strtok(NULL, ":"),CTRLM_DEVICE_UPDATE_PATH_LENGTH);
             g_message("IARM_event_sender entered case for PeripheralUpgradeEvent : %s and %s\r\n",firmwareInfo.firmwareLocation,firmwareInfo.firmwareNames);

             IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME,
            		 CTRLM_DEVICE_UPDATE_IARM_CALL_UPDATE_AVAILABLE,
                            (void *)&firmwareInfo,
                            sizeof(firmwareInfo));
          
        }
        #endif
    else if( !(g_ascii_strcasecmp(currentEventName->str,"USBMountChangedEvent")))
    {
        IARM_Bus_SYSMgr_EventData_t eventData;
        eventData.data.usbMountData.mounted = atoi(strtok(eventPayload, ":"));
        strncpy(eventData.data.usbMountData.device, strtok(NULL, ":"), sizeof(eventData.data.usbMountData.device) - 1);
        eventData.data.usbMountData.device[sizeof(eventData.data.usbMountData.device) - 1] = '\0';
        strncpy(eventData.data.usbMountData.dir, strtok(NULL, ":"), sizeof(eventData.data.usbMountData.dir) - 1);
        eventData.data.usbMountData.dir[sizeof(eventData.data.usbMountData.dir) - 1] = '\0';

        g_message("IARM_event_sender entered case for USBMountChangedEvent : %d %s %s",
                  eventData.data.usbMountData.mounted,
                  eventData.data.usbMountData.device,
                  eventData.data.usbMountData.dir);

        retCode = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME,
                (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_USB_MOUNT_CHANGED, (void *)&eventData, sizeof(eventData));

        g_message("IARM Event %d  retCode:%d", IARM_BUS_SYSMGR_EVENT_USB_MOUNT_CHANGED, retCode);
    }
#ifdef HAS_MAINTENANCE_MANAGER
    else if( !(g_ascii_strcasecmp(currentEventName->str,"MaintenanceMGR")))
    {
        g_message("IARM_event_sender entered for Maintenance Start time : %s\r\n",eventPayload);
        IARM_Bus_MaintMGR_EventData_t eventData;
        memset( &eventData, 0, sizeof(IARM_Bus_MaintMGR_EventData_t) );
        strncpy(eventData.data.startTimeUpdate.start_time, eventPayload, sizeof(eventData.data.startTimeUpdate.start_time)-1);
        eventData.data.startTimeUpdate.start_time[sizeof(eventData.data.startTimeUpdate.start_time)-1] = '\0';
        g_message("startTimeUpdate.start_time : %s\r\n", eventData.data.startTimeUpdate.start_time);
        IARM_Bus_BroadcastEvent(IARM_BUS_MAINTENANCE_MGR_NAME, (IARM_EventId_t)IARM_BUS_DCM_NEW_START_TIME_EVENT, (void *)&eventData, sizeof(eventData));
    }
#endif
#ifdef PLATFORM_SUPPORTS_RDMMGR
    else if( !(g_ascii_strcasecmp(currentEventName->str, "RDMAppStatusEvent")))
    {
        IARM_Bus_RDMMgr_EventData_t eventData;
        memset(&eventData, 0, sizeof(IARM_Bus_RDMMgr_EventData_t));

        /* Received payload is of the format - "pkg_name:<name>\npkg_version:<version>\npkg_inst_status:<status>\npkg_inst_path:<path>" */
        /* Parse package info from payload */
        char *nl_delim = "\n";
        char *temp_ptr = NULL;
        char *pkg_info = strdup(eventPayload);
        char *pkg_data = strtok(pkg_info, nl_delim);

        while (NULL != pkg_data)
        {
            temp_ptr = strchr(pkg_data, ':');
            if (NULL != temp_ptr)
            {
                temp_ptr++;
                if (NULL != strstr(pkg_data, RDM_PKG_NAME)) {
                    strncpy(eventData.rdm_pkg_info.pkg_name, temp_ptr, RDM_PKG_NAME_MAX_SIZE - 1);
                    eventData.rdm_pkg_info.pkg_name[RDM_PKG_NAME_MAX_SIZE - 1] = '\0';
                } else if (NULL != strstr(pkg_data, RDM_PKG_VERSION)) {
                    strncpy(eventData.rdm_pkg_info.pkg_version, temp_ptr, RDM_PKG_VERSION_MAX_SIZE - 1);
                    eventData.rdm_pkg_info.pkg_version[RDM_PKG_VERSION_MAX_SIZE - 1] = '\0';
                } else if (NULL != strstr(pkg_data, RDM_PKG_INST_PATH)) {
                    strncpy(eventData.rdm_pkg_info.pkg_inst_path, temp_ptr, RDM_PKG_INST_PATH_MAX_SIZE - 1);
                    eventData.rdm_pkg_info.pkg_inst_path[RDM_PKG_INST_PATH_MAX_SIZE - 1] = '\0';
                } else if (NULL != strstr(pkg_data, RDM_PKG_INST_STATUS)) {
                    eventData.rdm_pkg_info.pkg_inst_status = (IARM_RDMMgr_Status_t) atoi(temp_ptr);
                } else {
                    g_message("Unrecognized RDM package data: %s\n", pkg_data);
                }
            }
            pkg_data = strtok(NULL, nl_delim);
        }
        free(pkg_info);

        retCode = IARM_Bus_BroadcastEvent(IARM_BUS_RDMMGR_NAME, (IARM_EventId_t) IARM_BUS_RDMMGR_EVENT_APP_INSTALLATION_STATUS, (void *)&eventData, sizeof(eventData));
        g_message(">>>>> IARM %s  Event  = %d",(retCode == IARM_RESULT_SUCCESS) ? "SUCCESS" : "FAILURE", eventData.rdm_pkg_info.pkg_inst_status);
    }
#endif
    else if ( !(g_ascii_strcasecmp(currentEventName->str,"IpmodeEvent")))
    {
	    IARM_Bus_SYSMgr_EventData_t eventData;
	    eventData.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_IP_MODE;
	    eventData.data.systemStates.state = 1;
	    eventData.data.systemStates.error = 0;
	    strncpy(eventData.data.systemStates.payload,eventPayload,sizeof(eventData.data.systemStates.payload)-1);
	    eventData.data.systemStates.payload[sizeof(eventData.data.systemStates.payload)-1] = '\0';  //CID:136370 - Buffer size warning

        retCode = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME,
                (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, (void *)&eventData, sizeof(eventData));

        g_message("IARM Event %d  retCode:%d", IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, retCode);

    }
    else if ( !(g_ascii_strcasecmp(currentEventName->str,"usbdetected")))
    {
        IARM_Bus_SYSMgr_EventData_t eventData;
        char* token = strtok(eventPayload,":");
        if (! (g_ascii_strcasecmp(token,"add")))
        {
            eventData.data.usbData.inserted = 0;
        }
        if (! (g_ascii_strcasecmp(token,"remove")))
        {
            eventData.data.usbData.inserted = 1;
        }
        memset(eventData.data.usbData.vendor,'\0',sizeof(eventData.data.usbData.vendor));
        memset(eventData.data.usbData.productid,'\0',sizeof(eventData.data.usbData.productid));
        memset(eventData.data.usbData.devicename,'\0',sizeof(eventData.data.usbData.devicename));
        strncpy(eventData.data.usbData.vendor, strtok(NULL,":"), sizeof(eventData.data.usbData.vendor) - 1);
        strncpy(eventData.data.usbData.productid, strtok(NULL,":"), sizeof(eventData.data.usbData.productid) - 1);
        strncpy(eventData.data.usbData.devicename, strtok(NULL,":"), sizeof(eventData.data.usbData.devicename) - 1);

        retCode = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME,
                (IARM_EventId_t)IARM_BUS_SYSMGR_SYSSTATE_USB_DETECTED, (void *)&eventData, sizeof(eventData));

        g_message("IARM Event %d  retCode:%d", IARM_BUS_SYSMGR_SYSSTATE_USB_DETECTED, retCode);
    }
    else 
    {
		g_message("There are no matching IARM events for %s",currentEventName->str);
	}
	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	g_message("IARM_event_sender closing \r\n");
	return retCode;
}

/*
 * Parse Helper Function
 */
static bool parseArg(const char *arg, ArgType type, ArgValue *out)
{
    out->type = type;
    switch (type) {
        case ARG_INT:
            out->val.i = atoi(arg);
            return true;
        case ARG_BOOL:
            if (!strcasecmp(arg, "1") || !strcasecmp(arg, "true")) { out->val.b = true;  return true; }
            if (!strcasecmp(arg, "0") || !strcasecmp(arg, "false")){ out->val.b = false; return true; }
            return false;
        case ARG_STRING:
            out->val.s = arg;
            return true;
		default:
			g_message("parseArg Error unsupported Argument type \r\n");
			return false;
    }
}



static void handleCompositeInHotPlug(ArgValue *args)
{
    int port = INT_ARG(0);
    bool isPortConnected  = BOOL_ARG(1);

    g_message("handleCompositeInHotPlug: port=%d, isPortConnected=%s",
              port, isPortConnected ? "true" : "false");

    IARM_Bus_DSMgr_EventData_t composite_in_hpd_eventData;
    memset(&composite_in_hpd_eventData, 0, sizeof(composite_in_hpd_eventData));
	composite_in_hpd_eventData.data.composite_in_connect.port = port;
    composite_in_hpd_eventData.data.composite_in_connect.isPortConnected = isPortConnected;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
	                        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_COMPOSITE_IN_HOTPLUG,
	                        (void *)&composite_in_hpd_eventData,
	                        sizeof(composite_in_hpd_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleCompositeInSignalStatus(ArgValue *args)
{
    int port        = INT_ARG(0);
    int sigStatus   = INT_ARG(1);

    g_message("handleCompositeInSignalStatus: port=%d, sigStatus=%d", port, sigStatus);

	IARM_Bus_DSMgr_EventData_t composite_in_sigStatus_eventData;
    memset(&composite_in_sigStatus_eventData, 0, sizeof(composite_in_sigStatus_eventData));
    composite_in_sigStatus_eventData.data.composite_in_sig_status.port = port;
    composite_in_sigStatus_eventData.data.composite_in_sig_status.status = sigStatus;

	IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
			        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_COMPOSITE_IN_SIGNAL_STATUS,
			        (void *)&composite_in_sigStatus_eventData,
			        sizeof(composite_in_sigStatus_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleCompositeInStatus(ArgValue *args)
{
    int port           = INT_ARG(0);
    bool isPresented   = BOOL_ARG(1);

    g_message("handleCompositeInStatus: port=%d, enabled=%s",
              port, isPresented ? "true" : "false");

	IARM_Bus_DSMgr_EventData_t hdmi_in_status_eventData;
    memset(&hdmi_in_status_eventData, 0, sizeof(hdmi_in_status_eventData));
    hdmi_in_status_eventData.data.composite_in_status.port = port;
    hdmi_in_status_eventData.data.composite_in_status.isPresented = isPresented;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_COMPOSITE_IN_STATUS,
                                (void *)&hdmi_in_status_eventData,
                                sizeof(hdmi_in_status_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }

}

static void handleCompositeInVideoModeUpdate(ArgValue *args)
{
    int port              = INT_ARG(0);
    int pixelResolution   = INT_ARG(1);
    int interlaced        = INT_ARG(2);
    int frameRate         = INT_ARG(3);

    g_message("handleCompositeInVideoModeUpdate: port=%d, pixelResolution=%d interlaced=%d frameRate=%d", port, pixelResolution, interlaced , frameRate);
    IARM_Bus_DSMgr_EventData_t composite_in_videoMode_eventData;
    memset(&composite_in_videoMode_eventData, 0, sizeof(composite_in_videoMode_eventData));
    composite_in_videoMode_eventData.data.composite_in_video_mode.port = port;
    composite_in_videoMode_eventData.data.composite_in_video_mode.resolution.pixelResolution = pixelResolution;
    composite_in_videoMode_eventData.data.composite_in_video_mode.resolution.interlaced = interlaced;
    composite_in_videoMode_eventData.data.composite_in_video_mode.resolution.frameRate = frameRate;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_COMPOSITE_IN_VIDEO_MODE_UPDATE,
                                (void *)&composite_in_videoMode_eventData,
                                sizeof(composite_in_videoMode_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }

}


static void handleHdmiAllmEvent(ArgValue *args)
{
    int port      = INT_ARG(0);
    int allm_mode = INT_ARG(1);

    g_message("HdmiAllmEvent: port=%d, allm_mode=%d", port, allm_mode);

    IARM_Bus_DSMgr_EventData_t eventData;
    memset(&eventData, 0, sizeof(eventData));
    eventData.data.hdmi_in_allm_mode.port = port;
    eventData.data.hdmi_in_allm_mode.allm_mode = allm_mode;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                            (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_ALLM_STATUS,
                            &eventData,
                            sizeof(eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }

}


static void handleHdmiVrrEvent(ArgValue *args)
{
    int port       = INT_ARG(0);
    int vrr_status = INT_ARG(1);

    g_message("handleHdmiVrrEvent: port=%d, vrr_status=%d", port, vrr_status);

    IARM_Bus_DSMgr_EventData_t hdmi_in_vrrMode_eventData;
    memset(&hdmi_in_vrrMode_eventData, 0, sizeof(hdmi_in_vrrMode_eventData));
    hdmi_in_vrrMode_eventData.data.hdmi_in_vrr_mode.port = port;
    hdmi_in_vrrMode_eventData.data.hdmi_in_vrr_mode.vrr_type = vrr_status;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                            (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_VRR_STATUS,
                            (void *)&hdmi_in_vrrMode_eventData,
                            sizeof(hdmi_in_vrrMode_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiInVideoModeUpdate(ArgValue *args)
{
    int port              = INT_ARG(0);
    int pixelResolution   = INT_ARG(1);
    int interlaced        = INT_ARG(2);
    int frameRate         = INT_ARG(3);

    g_message("handleHdmiInVideoModeUpdate: port=%d, pixelResolution=%d interlaced=%d frameRate=%d", port, pixelResolution, interlaced , frameRate);
	IARM_Bus_DSMgr_EventData_t hdmi_in_videoMode_eventData;
    memset(&hdmi_in_videoMode_eventData, 0, sizeof(hdmi_in_videoMode_eventData));
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.port = port;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.pixelResolution = pixelResolution;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.interlaced = interlaced;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.frameRate = frameRate;


    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_VIDEO_MODE_UPDATE,
                                (void *)&hdmi_in_videoMode_eventData,
                                sizeof(hdmi_in_videoMode_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiInStatus(ArgValue *args)
{
    int port           = INT_ARG(0);
    bool isPresented   = BOOL_ARG(1);

    g_message("handleHdmiInStatus: port=%d, isPresented=%s",
              port, isPresented ? "true" : "false");

    IARM_Bus_DSMgr_EventData_t hdmi_in_status_eventData;
    memset(&hdmi_in_status_eventData, 0, sizeof(hdmi_in_status_eventData));
	hdmi_in_status_eventData.data.hdmi_in_status.port = port;
    hdmi_in_status_eventData.data.hdmi_in_status.isPresented = isPresented;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_STATUS,
                                (void *)&hdmi_in_status_eventData,
                                sizeof(hdmi_in_status_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiInSignalStatus(ArgValue *args)
{
    int port        = INT_ARG(0);
    int sigStatus   = INT_ARG(1);

    g_message("handleHdmiInSignalStatus: port=%d, sigStatus=%d", port, sigStatus);

    IARM_Bus_DSMgr_EventData_t hdmi_in_sigStatus_eventData;
    memset(&hdmi_in_sigStatus_eventData, 0, sizeof(hdmi_in_sigStatus_eventData));
    hdmi_in_sigStatus_eventData.data.hdmi_in_sig_status.port = port;
    hdmi_in_sigStatus_eventData.data.hdmi_in_sig_status.status = sigStatus;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
			        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_SIGNAL_STATUS,
			        (void *)&hdmi_in_sigStatus_eventData,
			        sizeof(hdmi_in_sigStatus_eventData));					
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }

}


static void handleHdmiInHotPlug(ArgValue *args)
{
    int port           = INT_ARG(0);
    bool isPlugged     = BOOL_ARG(1);

    g_message("handleHdmiInHotPlug: port=%d, isPlugged=%s",
              port, isPlugged ? "true" : "false");
    IARM_Bus_DSMgr_EventData_t hdmi_in_hpd_eventData;
    memset(&hdmi_in_hpd_eventData, 0, sizeof(hdmi_in_hpd_eventData));
    hdmi_in_hpd_eventData.data.hdmi_in_connect.port = port;
    hdmi_in_hpd_eventData.data.hdmi_in_connect.isPortConnected = isPlugged;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
	                        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG,
	                        (void *)&hdmi_in_hpd_eventData,
	                        sizeof(hdmi_in_hpd_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiHotPlug(ArgValue *args)
{
    bool isPlugged     = BOOL_ARG(0);

    g_message("handleHdmiHotPlug: isPlugged=%s",
                 isPlugged ? "true" : "false");
    IARM_Bus_DSMgr_EventData_t hdmi_in_hpd_eventData;
    memset(&hdmi_in_hpd_eventData, 0, sizeof(hdmi_in_hpd_eventData));

	hdmi_in_hpd_eventData.data.hdmi_hpd.event =  isPlugged;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
	                        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG,
	                        (void *)&hdmi_in_hpd_eventData,
	                        sizeof(hdmi_in_hpd_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiInAviContentType(ArgValue *args)
{
    int port                = INT_ARG(0);
    int avi_content_type    = INT_ARG(1);

    g_message("handleHdmiInAviContentType: port=%d, avi_content_type=%d", port, avi_content_type);

	IARM_Bus_DSMgr_EventData_t hdmi_in_contentType_eventData;

    memset(&hdmi_in_contentType_eventData, 0, sizeof(hdmi_in_contentType_eventData));
    hdmi_in_contentType_eventData.data.hdmi_in_content_type.port = port;
    hdmi_in_contentType_eventData.data.hdmi_in_content_type.aviContentType = avi_content_type;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_AVI_CONTENT_TYPE,
                                (void *)&hdmi_in_contentType_eventData,
                                sizeof(hdmi_in_contentType_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioOutHotPlug(ArgValue *args)
{
    int port_type           = INT_ARG(0);
    int port                = INT_ARG(1);
    bool isPortConnected    = BOOL_ARG(2);

    g_message("handleAudioOutHotPlug: port_type=%d, port=%d isPortConnected=%s",
                                port_type, port, isPortConnected ? "true" : "false");

    IARM_Bus_DSMgr_EventData_t audio_out_hpd_eventData;
    memset(&audio_out_hpd_eventData, 0, sizeof(audio_out_hpd_eventData));
    audio_out_hpd_eventData.data.audio_out_connect.portType = port_type;
    audio_out_hpd_eventData.data.audio_out_connect.uiPortNo = port;
    audio_out_hpd_eventData.data.audio_out_connect.isPortConnected = isPortConnected;
        
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                           (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_OUT_HOTPLUG,
                           (void *)&audio_out_hpd_eventData, 
                           sizeof(audio_out_hpd_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioFormatUpdate(ArgValue *args)
{
    int audioFormat         = INT_ARG(0);

    g_message("handleAudioFormatUpdate: audioFormat=%d", audioFormat);

    IARM_Bus_DSMgr_EventData_t audio_format_event_data;	
    memset(&audio_format_event_data, 0, sizeof(audio_format_event_data));	
	
    audio_format_event_data.data.AudioFormatInfo.audioFormat = audioFormat;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                           (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_FORMAT_UPDATE,
                           (void *)&audio_format_event_data,
                           sizeof(audio_format_event_data));	

    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioPrimaryLanguageChanged(ArgValue *args)
{
    const char *language = STR_ARG(0);	

    g_message("handleAudioPrimaryLanguageChanged: language=%s", language);

    IARM_Bus_DSMgr_EventData_t primary_language_event_data;
    memset(&primary_language_event_data, 0, sizeof(primary_language_event_data));		
    strncpy(primary_language_event_data.data.AudioLanguageInfo.audioLanguage, language, MAX_LANGUAGE_LEN-1);
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_PRIMARY_LANGUAGE_CHANGED,
                                   (void *)&primary_language_event_data,
                                   sizeof(primary_language_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleAudioSecondaryLanguageChanged(ArgValue *args)
{
    const char *language = STR_ARG(0);	

    g_message("handleAudioSecondaryLanguageChanged: language=%s", language);

    IARM_Bus_DSMgr_EventData_t secondary_language_event_data;
    memset(&secondary_language_event_data, 0, sizeof(secondary_language_event_data));		
    strncpy(secondary_language_event_data.data.AudioLanguageInfo.audioLanguage, language, MAX_LANGUAGE_LEN-1);
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_SECONDARY_LANGUAGE_CHANGED,
                                   (void *)&secondary_language_event_data,
                                   sizeof(secondary_language_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioFaderControl(ArgValue *args)
{
    int audioFader         = INT_ARG(0);

    g_message("handleAudioFaderControl: audioFader=%d", audioFader);

    IARM_Bus_DSMgr_EventData_t fader_control_event_data;
    memset(&fader_control_event_data, 0, sizeof(fader_control_event_data));	

    fader_control_event_data.data.FaderControlInfo.mixerbalance = audioFader;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_FADER_CONTROL_CHANGED,
                                   (void *)&fader_control_event_data,
                                   sizeof(fader_control_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioMixingChanged(ArgValue *args)
{
    int audiomixval        = INT_ARG(0);

    g_message("handleAudioMixingChanged: audiomixval=%d", audiomixval);

    IARM_Bus_DSMgr_EventData_t associated_audio_mixing_event_data;
    memset(&associated_audio_mixing_event_data, 0, sizeof(associated_audio_mixing_event_data));	
    associated_audio_mixing_event_data.data.AssociatedAudioMixingInfo.mixing = audiomixval;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_ASSOCIATED_AUDIO_MIXING_CHANGED,
                                   (void *)&associated_audio_mixing_event_data,
                                   sizeof(associated_audio_mixing_event_data));

    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioPortState(ArgValue *args)
{
    int audioportstate        = INT_ARG(0);

    g_message("handleAudioPortState: audioportstate=%d", audioportstate);

    IARM_Bus_DSMgr_EventData_t audio_portstate_event_data;
    memset(&audio_portstate_event_data, 0, sizeof(audio_portstate_event_data));	
	audio_portstate_event_data.data.AudioPortStateInfo.audioPortState = audioportstate;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                           (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_PORT_STATE,
                           (void *)&audio_portstate_event_data,
                           sizeof(audio_portstate_event_data));
						   
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioMode(ArgValue *args)
{
    int audiomode       = INT_ARG(0);
    int audiotype       = INT_ARG(1);
	
    g_message("handleAudioMode: audiomode=%d audiotype=%d", audiomode,audiotype);

    IARM_Bus_DSMgr_EventData_t eventData;
    memset(&eventData, 0, sizeof(eventData));	
    eventData.data.Audioport.mode = audiomode;
    eventData.data.Audioport.type = audiotype;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,(IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_MODE,(void *)&eventData, sizeof(eventData));	   
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleDisplayFrameRatePreChange(ArgValue *args)
{
    const char *framerate = STR_ARG(0);	

    g_message("handleDisplayFrameRatePreChange: framerate=%s", framerate);

    IARM_Bus_DSMgr_EventData_t framerate_event_data;
    memset(&framerate_event_data, 0, sizeof(framerate_event_data));		
    /* The max value defined in the header file is 20 hard coded */
    strncpy(framerate_event_data.data.DisplayFrameRateChange.framerate, framerate, 19);
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_DISPLAY_FRAMRATE_PRECHANGE,
                                   (void *)&framerate_event_data,
                                   sizeof(framerate_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleDisplayFrameRatePostChange(ArgValue *args)
{
    const char *framerate = STR_ARG(0);	

    g_message("handleDisplayFrameRatePostChange: framerate=%s", framerate);

    IARM_Bus_DSMgr_EventData_t framerate_event_data;
    memset(&framerate_event_data, 0, sizeof(framerate_event_data));		
    /* The max value defined in the header file is 20 hard coded */
    strncpy(framerate_event_data.data.DisplayFrameRateChange.framerate, framerate, 19);
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_DISPLAY_FRAMRATE_POSTCHANGE,
                                   (void *)&framerate_event_data,
                                   sizeof(framerate_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleAtmosCapsChanged(ArgValue *args)
{
    int atmoscaps      = INT_ARG(0);
    bool status        = BOOL_ARG(1);

    g_message("handleAtmosCapsChanged: atmoscaps=%d, status=%s",
              atmoscaps, status ? "true" : "false");

    IARM_Bus_DSMgr_EventData_t atmos_caps_change_event_data;
    memset(&atmos_caps_change_event_data, 0, sizeof(atmos_caps_change_event_data));
    atmos_caps_change_event_data.data.AtmosCapsChange.caps = atmoscaps;
    atmos_caps_change_event_data.data.AtmosCapsChange.status = status;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                           (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_ATMOS_CAPS_CHANGED,
                           (void *)&atmos_caps_change_event_data,
                           sizeof(atmos_caps_change_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleAudioLevelChanged(ArgValue *args)
{
    int audiolevel       = INT_ARG(0);

    g_message("handleAudioLevelChanged: audiolevel=%d", audiolevel);

    IARM_Bus_DSMgr_EventData_t audio_level_event_data;
    memset(&audio_level_event_data, 0, sizeof(audio_level_event_data));	
    audio_level_event_data.data.AudioLevelInfo.level = audiolevel;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_AUDIO_LEVEL_CHANGED,
                                   (void *)&audio_level_event_data,
                                   sizeof(audio_level_event_data));

    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleVideoFormatUdate(ArgValue *args)
{
    int videoformat       = INT_ARG(0);

    g_message("handleVideoFormatUdate: videoformat=%d", videoformat);

    IARM_Bus_DSMgr_EventData_t video_format_event_data;
    memset(&video_format_event_data, 0, sizeof(video_format_event_data));	
    video_format_event_data.data.VideoFormatInfo.videoFormat = videoformat;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_VIDEO_FORMAT_UPDATE,
                                   (void *)&video_format_event_data,
                                   sizeof(video_format_event_data));

    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleEventZoomSetting(ArgValue *args)
{
    int zoomvalue      = INT_ARG(0);

    g_message("handleEventZoomSetting: zoomvalue=%d", zoomvalue);

    IARM_Bus_DSMgr_EventData_t zoom_format_event_data;
    memset(&zoom_format_event_data, 0, sizeof(zoom_format_event_data));	
    zoom_format_event_data.data.dfc.zoomsettings = zoomvalue;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_VIDEO_FORMAT_UPDATE,
                                   (void *)&zoom_format_event_data,
                                   sizeof(zoom_format_event_data));

    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleDisplayResolutionPreChange(ArgValue *args)
{
    int height       = INT_ARG(0);
    int width        = INT_ARG(1);

    g_message("handleDisplayResolutionPreChange: height=%d width=%d", height,width);

    IARM_Bus_DSMgr_EventData_t res_event_data;
    memset(&res_event_data, 0, sizeof(res_event_data));		

    res_event_data.data.resn.width = width;
    res_event_data.data.resn.height = height;
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_RES_PRECHANGE,
                                   (void *)&res_event_data,
                                   sizeof(res_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleDisplayResolutionPostChange(ArgValue *args)
{
    int height       = INT_ARG(0);
    int width        = INT_ARG(1);

    g_message("handleDisplayResolutionPostChange: height=%d width=%d", height,width);

    IARM_Bus_DSMgr_EventData_t res_event_data;
    memset(&res_event_data, 0, sizeof(res_event_data));		

    res_event_data.data.resn.width = width;
    res_event_data.data.resn.height = height;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE,
                                   (void *)&res_event_data,
                                   sizeof(res_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void handleHdmiInAvLatency(ArgValue *args)
{
    int audio_op_delay        = INT_ARG(0);
    int video_latency         = INT_ARG(1);

    g_message("handleHdmiInAvLatency: audio_op_delay=%d video_latency=%d", audio_op_delay,video_latency);

    IARM_Bus_DSMgr_EventData_t hdmi_in_av_latency_eventData;
    memset(&hdmi_in_av_latency_eventData, 0, sizeof(hdmi_in_av_latency_eventData));		

    hdmi_in_av_latency_eventData.data.hdmi_in_av_latency.audio_output_delay = audio_op_delay;
    hdmi_in_av_latency_eventData.data.hdmi_in_av_latency.video_latency = video_latency;

    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_AV_LATENCY,
                                   (void *)&hdmi_in_av_latency_eventData,
                                   sizeof(hdmi_in_av_latency_eventData));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }	
}


// -----------------------------
// Dispatcher
// -----------------------------
static void handleIARMEvents(int argc, char *argv[])
{
    const char *eventName = argv[1];

    for (int i = 0; i < iarmEventTableSize; i++) {
        IARM_EventEntry *entry = &iarmEventTable[i];
        if (!strcmp(eventName, entry->name)) {

            if (argc - 2 != entry->argc_expected) {
                g_message("Error: %s expects %d args, got %d\n",
                       entry->name, entry->argc_expected, argc - 2);
                return;
            }

            ArgValue args[MAX_ARG_NO_TYPE];
            for (int j = 0; j < entry->argc_expected; j++) {
                if (!parseArg(argv[j+2], entry->arg_types[j], &args[j])) {
                    g_message("Error: invalid arg %d ('%s') for event %s\n",
                           j+1, argv[j+2], entry->name);
                    return;
                }
            }

            /* Debug: print parsed args */
            g_message("[IARM] Dispatching %s with %d args\n", eventName, entry->argc_expected);
            for (int j = 0; j < entry->argc_expected; j++) {
                switch (args[j].type) {
                    case ARG_INT:    g_message("  Arg[%d] INT  = %d\n", j, args[j].val.i); break;
                    case ARG_BOOL:   g_message("  Arg[%d] BOOL = %s\n", j, args[j].val.b ? "true" : "false"); break;
                    case ARG_STRING: g_message("  Arg[%d] STR  = %s\n", j, args[j].val.s); break;
					default:  g_message("handleIARMEvents Error Unsupported Argument type \r\n");
                }
            }
            IARM_Bus_Init("SimulateDSMgrEvent");
            IARM_Bus_Connect();
            entry->handler(args);
            IARM_Bus_Disconnect();
            IARM_Bus_Term();
            return;
        }
    }
    g_message("Error: Unknown IARM DSMgr event: %s\n", eventName);
	printUsage(argv[0]);
}

static void handleEventRxSense(ArgValue *args)
{
    int rxsense        = INT_ARG(0);

    g_message("handleAudioPortState: audioportstate=%d", rxsense);

    IARM_Bus_DSMgr_EventData_t rxsense_event_data;
    memset(&rxsense_event_data, 0, sizeof(rxsense_event_data));	
	rxsense_event_data.data.hdmi_rxsense.status = rxsense;
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                           (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_RX_SENSE,
                           (void *)&rxsense_event_data,
                           sizeof(rxsense_event_data));
						   
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}

static void handleEventHdcpStatus(ArgValue *args)
{
    const char *value = STR_ARG(0);	

    g_message("handleEventHdcpStatus: value=%s", value);

    IARM_Bus_DSMgr_EventData_t hdcp_event_data;
    memset(&hdcp_event_data, 0, sizeof(hdcp_event_data));		
 
    IARM_Result_t rc = IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                   (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
                                   (void *)&hdcp_event_data,
                                   sizeof(hdcp_event_data));
    if (rc != IARM_RESULT_SUCCESS)
    {
       g_warning("IARM_Bus_BroadcastEvent failed for %s: rc=%d", __func__, rc);
    }
}


static void printUsage(const char *prog)
{
    g_message("Usage Applicable only for IARM Events:\n");
    g_message("  %s DSMgr_CompositeInHotPlug <port> <true/false>", prog);
    g_message("  %s DSMgr_CompositeInSignalStatus <port> <signalvalue>", prog);
    g_message("  %s DSMgr_CompositeInStatus <port> <true/false>", prog);
    g_message("  %s DSMgr_CompositeInVideoModeUpdate <port> <pixelresolution> <interlaced> <frameRate>", prog);
    g_message("  %s DSMgr_HdmiAllmEvent <port> <allm_mode>", prog);
    g_message("  %s DSMgr_HdmiInVideoModeUpdate <port> <pixelresolution> <interlaced> <frameRate>", prog);
    g_message("  %s DSMgr_HdmiVrrEvent <port> <vrrvalue>", prog);
    g_message("  %s DSMgr_HdmiInStatus <port> <ispresented>", prog);
    g_message("  %s DSMgr_HdmiInSignalStatus <port> <signalvalue>", prog);
    g_message("  %s DSMgr_HdmiInHotPlug <port> <true/false>", prog);
    g_message("  %s DSMgr_HdmiInAviContentType <port> <avi_content_type>", prog);
    g_message("  %s DSMgr_AudioOutHotPlug <port_type> <port> <isconnected:true/false>", prog);
    g_message("  %s DSMgr_AudioFormatUpdate <Audio_format>", prog);
    g_message("  %s DSMgr_AudioPrimaryLanguageChanged <lang string>", prog);
    g_message("  %s DSMgr_AudioSecondaryLanguageChanged <lang string>", prog);	
    g_message("  %s DSMgr_AudioFaderControl <Audio_faderval>", prog);	
    g_message("  %s DSMgr_AudioMixingChanged <Audio_mixingval>", prog);
    g_message("  %s DSMgr_AudioPortState <Audio_PortStateVal>", prog);
    g_message("  %s DSMgr_AudioMode <porttype> <audiomode>", prog);
    g_message("  %s DSMgr_DisplayFrameRatePreChange <framerate_string>", prog);
    g_message("  %s DSMgr_DisplayFrameRatePostChange <framerate_string>", prog);
    g_message("  %s DSMgr_AtmosCapsChanged <atmosCaps> <true/false>", prog);
    g_message("  %s DSMgr_EventRxSense <rxsense_value>", prog);	
    g_message("  %s DSMgr_EventZoomSettings <zoom_value>", prog);
    g_message("  %s DSMgr_HdmiHotPlug <true/false>", prog);
    g_message("  %s DSMgr_AudioLevelChanged <audio_value>", prog);	
    g_message("  %s DSMgr_VideoFormatUpdate <video_format_value_in_binary_bit_position>", prog);
    g_message("  %s DSMgr_HdmiInAvLatency <audio_output_delay> <video_latency>", prog);		
    g_message("  %s handleEventHdcpStatus <string_none>", prog);		
}