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
        g_message("Usage: %s <event name > <event status> \n",argv[0]);
        g_message("Usage: %s CustomEvent <event stateId> <event state> <event error> \n",argv[0]);
        g_message("(%d)\n",argc );
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


