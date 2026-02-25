/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "libIBus.h"
#include "pwrMgr.h"


#define PADDING_SIZE 32

/*LED settings*/
typedef struct _PWRMgr_LED_Settings_t{
    unsigned int brightness;
    unsigned int color;
}PWRMgr_LED_Settings_t;

typedef struct _PWRMgr_Settings_t {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    IARM_Bus_PWRMgr_PowerState_t powerState;
    PWRMgr_LED_Settings_t ledSettings;    
    uint32_t deep_sleep_timeout;    
    char padding[PADDING_SIZE];
} PWRMgr_Settings_t;

void usage ()
{
    printf ("\nUsage: 'QueryPowerState [CMD]'\n");
    printf ("\tCMDs are,\n");
    printf ("\t\t -h       -> Help\n");
    printf ("\t\t -c       -> Box state from iARM Cache\n");
    printf ("\t\t No CMD will read the iARM state from '/opt'\n");

    printf ("\n\tOutput will be,\n");
    printf ("\t\t\t ON         -> Box is in Active Mode\n");
    printf ("\t\t\t STANDBY    -> Box is in Standby Mode\n");
    printf ("\t\t\t LIGHTSLEEP -> Box is in Light Sleep Standby Mode\n");
    printf ("\t\t\t DEEPSLEEP  -> Box is in Deep Sleep Standby Mode\n");
    printf ("\t\t\t OFF        -> Box id OFF\n");
}

/**
 * Test application to check whether the box is in standby or not.
 * This has been developed to resolve, XONE-4598
 */
int main(int argc, char *argv[])
{
    PWRMgr_Settings_t pwrSettings;
    int ret = 0;
    const char *settingsFile = "/opt/uimgr_settings.bin";
   
	memset(&pwrSettings,0,sizeof(PWRMgr_Settings_t));

    if (argc > 1)
    {
        if (argv[1][1] == 'c')
        {
            IARM_Bus_PWRMgr_GetPowerState_Param_t param;

            IARM_Bus_Init("iARMQueryPower_1tool");
            IARM_Bus_Connect();      
	
            /** Query current Power state  */
            if (IARM_RESULT_SUCCESS ==  IARM_Bus_Call(IARM_BUS_PWRMGR_NAME,
                                            IARM_BUS_PWRMGR_API_GetPowerState,
                                            (void *)&param,
                                            sizeof(param)))
            {

                if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_OFF)
                    printf ("OFF");
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY)
                    printf ("STANDBY");
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_ON)
                    printf ("ON");
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP)
                    printf ("LIGHTSLEEP");
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
                    printf ("DEEPSLEEP");
                else
                    printf ("Unknown");
            }
            else
                printf ("Unknown");
			
			IARM_Bus_Disconnect();
			IARM_Bus_Term();
		}
        else if (argv[1][1] == 'h')
        {
            usage();
        }
    }
    else
    {
        int fd = open(settingsFile, O_RDONLY);

        if (fd > 0) {
            lseek(fd, 0, SEEK_SET);
            
			ret = read(fd, &pwrSettings, (sizeof(PWRMgr_Settings_t) - PADDING_SIZE) );

            close (fd);
        }

        if (ret > 0)
        {
            if (pwrSettings.powerState == IARM_BUS_PWRMGR_POWERSTATE_OFF)
                printf ("OFF");
            else if (pwrSettings.powerState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY)
                printf ("STANDBY");
            else if (pwrSettings.powerState == IARM_BUS_PWRMGR_POWERSTATE_ON)
                printf ("ON");
            else if (pwrSettings.powerState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP)
                printf ("LIGHTSLEEP");
            else if (pwrSettings.powerState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
                printf ("DEEPSLEEP");
            else 
                printf ("Unknown Power state");
        }
        else
            printf ("Error in reading PWRMgr settings File");
    }
	 printf ("\n");
    return 0;
}
