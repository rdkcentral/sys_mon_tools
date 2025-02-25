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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// This include is still required for data types to read cached power status residing in /opt/uimgr_settings.bin
// TODO: after IARM PwrMgr logic is moved to PowerManager plugin refactor this code and remove this header file too.
#include "pwrMgr.h"

#include "power_controller.h"

#define PADDING_SIZE 32

/*LED settings*/
typedef struct _PWRMgr_LED_Settings_t {
    unsigned int brightness;
    unsigned int color;
} PWRMgr_LED_Settings_t;

typedef struct _PWRMgr_Settings_t {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    IARM_Bus_PWRMgr_PowerState_t powerState;
    PWRMgr_LED_Settings_t ledSettings;
    uint32_t deep_sleep_timeout;
    char padding[PADDING_SIZE];
} PWRMgr_Settings_t;

void usage()
{
    printf("\nUsage: 'QueryPowerState [CMD]'\n");
    printf("\tCMDs are,\n");
    printf("\t\t -h       -> Help\n");
    printf("\t\t -c       -> Box state from PowerManager plugin\n");
    printf("\t\t No CMD will read the iARM state from '/opt'\n");

    printf("\n\tOutput will be,\n");
    printf("\t\t\t ON         -> Box is in Active Mode\n");
    printf("\t\t\t STANDBY    -> Box is in Standby Mode\n");
    printf("\t\t\t LIGHTSLEEP -> Box is in Light Sleep Standby Mode\n");
    printf("\t\t\t DEEPSLEEP  -> Box is in Deep Sleep Standby Mode\n");
    printf("\t\t\t OFF        -> Box id OFF\n");
}

/**
 * Test application to check whether the box is in standby or not.
 * This has been developed to resolve, XONE-4598
 */
int main(int argc, char* argv[])
{
    int ret = 0;

    if (argc > 1) {
        if (argv[1][1] == 'c') {
            uint32_t res = 0;
            PowerController_PowerState_t curState = POWER_STATE_UNKNOWN, previousState = POWER_STATE_UNKNOWN;

            PowerController_Init();

            /** Query current Power state  */
            res = PowerController_GetPowerState(&curState, &previousState);

            if (POWER_CONTROLLER_ERROR_NONE == res) {

                if (POWER_STATE_OFF == curState) {
                    printf("OFF\n");
                } else if (POWER_STATE_STANDBY == curState) {
                    printf("STANDBY\n");
                } else if (POWER_STATE_ON == curState) {
                    printf("ON\n");
                } else if (POWER_STATE_STANDBY_LIGHT_SLEEP == curState) {
                    printf("LIGHTSLEEP\n");
                } else if (POWER_STATE_STANDBY_DEEP_SLEEP == curState) {
                    printf("DEEPSLEEP\n");
                } else {
                    printf("Unknown\n");
                }
            } else if (POWER_CONTROLLER_ERROR_UNAVAILABLE == res) {
                printf("Error :: PowerManager plugin unavailable\n");
            } else {
                printf("Error :: Unknown\n");
            }

            /* Dispose closes RPC conn, do not make any power manager calls after this */
            PowerController_Term();

        } else if (argv[1][1] == 'h') {
            usage();
        }
    } else {
        PWRMgr_Settings_t pwrSettings;
        const char* settingsFile = "/opt/uimgr_settings.bin";

        int fd = open(settingsFile, O_RDONLY);

        memset(&pwrSettings, 0, sizeof(PWRMgr_Settings_t));

        if (fd > 0) {
            lseek(fd, 0, SEEK_SET);
            ret = read(fd, &pwrSettings, (sizeof(PWRMgr_Settings_t) - PADDING_SIZE));

            close(fd);
        }

        if (ret > 0) {
            if (IARM_BUS_PWRMGR_POWERSTATE_OFF == pwrSettings.powerState) {
                printf("OFF");
            } else if (IARM_BUS_PWRMGR_POWERSTATE_STANDBY == pwrSettings.powerState) {
                printf("STANDBY");
            } else if (IARM_BUS_PWRMGR_POWERSTATE_ON == pwrSettings.powerState) {
                printf("ON");
            } else if (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP == pwrSettings.powerState) {
                printf("LIGHTSLEEP");
            } else if (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == pwrSettings.powerState) {
                printf("DEEPSLEEP");
            } else {
                printf("Unknown Power state");
            }
        } else {
            printf("Error in reading PWRMgr settings File");
        }

        printf("\n");
    }
    return 0;
}
