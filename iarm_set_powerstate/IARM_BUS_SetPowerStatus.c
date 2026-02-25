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
#include <string.h>

#include "powermanager_client.h"

const char* pPowerON = "ON";
const char* pPowerOFF = "OFF";
const char* pPowerStandby = "STANDBY";
const char* pPowerLigtSleep = "LIGHTSLEEP";
const char* pPowerDeepSleep = "DEEPSLEEP";

void usage()
{
    printf("\nUsage: 'SetPowerState [ON | STANDBY | LIGHTSLEEP | DEEPSLEEP | OFF ] with PowerManager plugin'\n");
    printf("\t\t ON         -> Set to Active Mode\n");
    printf("\t\t STANDBY    -> Set to Standby Mode\n");
    printf("\t\t LIGHTSLEEP -> Set to LIGHT Sleep Standby mode\n");
    printf("\t\t DEEPSLEEP  -> Set to DEEP Sleep Standby mode\n");
    printf("\t\t OFF        -> Set to OFF\n");
}

/**
 * Test application to check whether the box is in standby or not.
 * This has been developed to resolve, XONE-4598
 */
int main(int argc, char* argv[])
{
    PowerManager_PowerState_t powerstate = POWER_STATE_ON;

    if (argc < 2) {
        usage();
    } else if (strncasecmp(pPowerON, argv[1], strlen(pPowerON)) == 0) {
        powerstate = POWER_STATE_ON;
        printf("ON Request...\n");
    } else if (strncasecmp(pPowerStandby, argv[1], strlen(pPowerStandby)) == 0) {
        powerstate = POWER_STATE_STANDBY;
        printf("STANDBY Request...\n");
    } else if (strncasecmp(pPowerLigtSleep, argv[1], strlen(pPowerLigtSleep)) == 0) {
        powerstate = POWER_STATE_STANDBY_LIGHT_SLEEP;
        printf("Light Sleep Request...\n");
    } else if (strncasecmp(pPowerDeepSleep, argv[1], strlen(pPowerDeepSleep)) == 0) {
        powerstate = POWER_STATE_STANDBY_DEEP_SLEEP;
        printf("Deep Sleep Request...\n");
    } else if (strncasecmp(pPowerOFF, argv[1], strlen(pPowerOFF)) == 0) {
        powerstate = POWER_STATE_OFF;
        printf("OFF Request...\n");
    } else {
        usage();
    }

    if ((POWER_STATE_ON == powerstate)
        || (POWER_STATE_OFF == powerstate)
        || (POWER_STATE_STANDBY == powerstate)
        || (POWER_STATE_STANDBY_LIGHT_SLEEP == powerstate)
        || (POWER_STATE_STANDBY_DEEP_SLEEP == powerstate)) {

        int keyCode = 0;

        /** Query current Power state  */
        if (!PowerManager_SetPowerState(keyCode, powerstate, "sys_mon_tool[SetPowerState]")) {
            printf("SetPowerState :: Success \n");
        } else {
            printf("SetPowerState :: Failed \n");
        }

        /* Dispose closes RPC conn, do not make any power manager calls after this */
        PowerManager_Dispose();
    }
    return 0;
}
