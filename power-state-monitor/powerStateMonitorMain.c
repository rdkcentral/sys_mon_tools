/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>      /* Errors */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <glib.h>
#include <time.h>
#include <unistd.h>

#include "power_controller.h"

#define TMP_LIGHTSLEEP_ON "/tmp/.lightsleep_on"
#define TMP_POWER_ON "/tmp/.power_on"
#define LOG_FILE_NAME "/lightsleep.log"

static PowerController_PowerState_t gpowerState = POWER_STATE_ON;

/* Function Declarations */
static void _lightsleepEventHandler (const PowerController_PowerState_t currentState,
                                      const PowerController_PowerState_t newState, void* userdata);
static void* lightsleep_monitor(void *arg);

// Helper to check if file exists
static int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Helper to get timestamp string
static void get_timestamp(char *buf, size_t buflen) {
    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) == NULL) {
        if (buflen > 0) {
            buf[0] = '\0';
        }
        return;
    }
     strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm_info);
}

static void* lightsleep_monitor(void *arg) {

    FILE *log = NULL;

    if (file_exists(TMP_LIGHTSLEEP_ON)) {
        return NULL;
    }

    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s%s", "/tmp", LOG_FILE_NAME);

    // Touch /tmp/.lightsleep_on
    FILE *touch = fopen(TMP_LIGHTSLEEP_ON, "w");
    if (touch) fclose(touch);
 
    if (file_exists(TMP_POWER_ON)) {
            log = fopen(log_file, "a");
            if (log) {
                char ts[32];
                get_timestamp(ts, sizeof(ts));
                fprintf(log, "%s Box is in Power ON mode, journalctl will sync the logs..!\n", ts);
                fclose(log);
            }
            if(remove(TMP_LIGHTSLEEP_ON) != 0) {
                printf("Error deleting lightsleep file\n");
            }
            return NULL;
    }

    log = fopen(log_file, "a");
    if (log) {
        char ts[32];
        get_timestamp(ts, sizeof(ts));
        fprintf(log, "%s Starting the lightsleep monitoring..!\n", ts);
        fclose(log);
    }

    while (1) {
        sleep(60);
        if (file_exists(TMP_POWER_ON)) {
                log = fopen(log_file, "a");
                if (log) {
                    char ts[32];
                    get_timestamp(ts, sizeof(ts));
                    fprintf(log, "%s Box is in Power ON mode from STANDBY..! exiting\n", ts);
                    fclose(log);
                }
                if(remove(TMP_LIGHTSLEEP_ON) != 0) {
                    printf("Error deleting lightsleep file\n");
                }
                return NULL;
        }
    }
    return NULL;
}

/*****************************************************************
 * Input Arguments: int
 * Output Arguments: void
 * Description: This is to set the .standby/poweron flag based on
                the power MODE
*****************************************************************/
static void setModeSettings(int mode)
{
    int status=0;
    FILE *fptr = NULL;
    char setFlag[30] = {0};
    char deleteFlag[30] = {0};
    switch (mode){
       case 1:
           strcpy(setFlag, "/tmp/.power_on");
           strcpy(deleteFlag,"/tmp/.standby");
           printf("setModeSettings: Power ON Mode \n");
           break;
       case 0:
           strcpy(setFlag,"/tmp/.standby");
           strcpy(deleteFlag,"/tmp/.power_on");
           printf("setModeSettings: STANDBY Mode \n");
           {
               pthread_t lightsleep_tid;
               if (pthread_create(&lightsleep_tid, NULL, lightsleep_monitor, NULL) == 0) {
                   pthread_detach(lightsleep_tid);
               } else {
                   printf("setModeSettings: Failed to create lightsleep_monitor thread\n");
               }
           }
           break;
       default:
           printf("setModeSettings: Unknown Argument..!\n");
           return;
    }
    fptr = fopen(setFlag, "w+");
    if(fptr == NULL){ //if file does not exist, create it
         printf("Failure: Not able to create a file (%s)\n",setFlag);
    }
    else{
         printf("Success: File (%s) created successfully..!\n",setFlag);
         fclose(fptr);
    }
    /* Deletes the standby/poweron file */
    status = remove(deleteFlag);
    if(status == 0){
         printf("Success: File (%s) deleted successfully\n",deleteFlag);
    }
}

/*****************************************************************
 * Function Name: _lightsleepEventHandler
 * Input Parameters:
 *   - currentState: The previous power state (PowerController_PowerState_t)
 *   - newState: The new power state after the transition (PowerController_PowerState_t)
 *   - userdata: Optional user data pointer (void*)
 * Output Parameters: void
 * Description:
 *   Callback function registered with PowerController to handle
 *   power state transitions. This function is invoked whenever
 *   the power state changes, and updates system flags or performs
 *   actions based on the new power state.
 *****************************************************************/
static void _lightsleepEventHandler (const PowerController_PowerState_t currentState,
                                      const PowerController_PowerState_t newState, void* userdata)
{
	printf("Entering _lightsleepEventHandler:State Changed currentState: %d, newState: %d \n",
			currentState, newState);
	gpowerState = newState;

    if(gpowerState == POWER_STATE_ON)
    {
         setModeSettings (1);
    }
    else
    {
         setModeSettings (0);
    }
	printf("Exiting _lightsleepEventHandler..\n");
}
/*****************************************************************
 * Function Name: power_monitor_init
 * Input Parameters: (void)
 * Output Parameters: void
 * Description: Set the power flag during bootup and 
                Detect the power transitions
*****************************************************************/
int power_monitor_init(void)
{
    uint32_t res = 0;
    PowerController_PowerState_t curState = POWER_STATE_UNKNOWN, previousState = POWER_STATE_UNKNOWN;

    PowerController_Init();

    while (!PowerController_IsOperational()) {
        uint32_t status = PowerController_Connect();

        if (POWER_CONTROLLER_ERROR_NONE == status) {
            printf("\nSuccess :: Connect\n");
            break;
        } else if (POWER_CONTROLLER_ERROR_UNAVAILABLE == status) {
            printf("\nFailed :: Connect :: Thunder is UNAVAILABLE\n");
        } else if (POWER_CONTROLLER_ERROR_NOT_EXIST == status) {
            printf("\nFailed :: Connect :: PowerManager is UNAVAILABLE\n");
        } else {
            // Do nothing
        }
        usleep(100 * 1000); // 100ms
    }

    res = PowerController_GetPowerState(&curState, &previousState);

    if (POWER_CONTROLLER_ERROR_NONE == res) {
        if (POWER_STATE_OFF == curState) {
            printf("OFF Mode\n");
            setModeSettings(0);
        } else if (POWER_STATE_STANDBY == curState) {
            printf("STANDBY Mode\n");
            setModeSettings(0);
        } else if (POWER_STATE_ON == curState) {
            printf("ON Mode\n");
            setModeSettings(1);
        } else if (POWER_STATE_STANDBY_LIGHT_SLEEP == curState) {
            printf("LIGHTSLEEP Mode\n");
            setModeSettings(0);
        } else if (POWER_STATE_STANDBY_DEEP_SLEEP == curState) {
            printf("DEEPSLEEP Mode\n");
            setModeSettings(0);
        } else {
            printf("Unknown Power state\n");
        }
    } else if (POWER_CONTROLLER_ERROR_UNAVAILABLE == res) {
        printf("Error :: PowerManager plugin unavailable\n");
    } else {
        printf("Error :: Unknown\n");
    }

    return 0;
}
/*********************************************************
 * Function Name: main ()
 * Input Parameters: int argc, char *argv[]
 * Output Parameters: int
 * Description: Registers the lightsleep thread functions
*********************************************************/
int main(int argc, char *argv[])
{
    GAsyncQueue *msgQueuePtr=NULL;

    power_monitor_init();
    
    /* Register the event callback function..! */
    printf("%s : Registering Callback for Power Mode Change Notification..!\n", __FUNCTION__);
    PowerController_RegisterPowerModeChangedCallback(_lightsleepEventHandler, NULL);
    
     /* Create the async queue once and keep the process alive by popping from it */
    msgQueuePtr = g_async_queue_new ();
    if ( msgQueuePtr == NULL ){
        printf("Not able to create a queue..!");
        return 1;
    }

    /* This call will make the process alive */
    while (true)
    {
        printf("Wait Call for message pop..!\n");
        g_async_queue_pop (msgQueuePtr);
    }
}

