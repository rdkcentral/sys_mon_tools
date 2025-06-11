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
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#include "power_controller.h"

#define ARRAY_SIZE 10

const char* pPowerON = "ON";
const char* pPowerOFF = "OFF";
const char* pPowerStandby = "STANDBY";
const char* pPowerLigtSleep = "LIGHTSLEEP";
const char* pPowerDeepSleep = "DEEPSLEEP";
const char* pNOP = "NOP";

static void usage()
{
    printf("\nUsage: 'SetPowerState [OPTIONS] [ON | STANDBY | LIGHTSLEEP | DEEPSLEEP | OFF | NOP] with PowerManager plugin\n");
    printf("\t\t ON         -> Set to Active Mode\n");
    printf("\t\t STANDBY    -> Set to Standby Mode\n");
    printf("\t\t LIGHTSLEEP -> Set to LIGHT Sleep Standby mode\n");
    printf("\t\t DEEPSLEEP  -> Set to DEEP Sleep Standby mode\n");
    printf("\t\t OFF        -> Set to OFF\n");
    printf("\t\t NOP        -> No operation, to support test scenarios with parallel `SetPowerState` run\n\n");
    printf("\tOptions:\n");
    printf("\t  --client <CLIENT_NAME>   Specify client name (e.g., C1, C2).\n");
    printf("\t  --ack <ACK_DELAY>        Delay to acknowledge power change in seconds (-1: no ack, 0: quick ack, >0: delayed ack; default: -1).\n");
    printf("\t  --delay <D1,D2,D3>       Delay for power change in seconds, as CSV (default: no delay).\n");
    printf("\t                           Each delay corresponds to `DelayPowerModeChangeBy` call\n");
    printf("\t  --await <SECONDS>        Wait at least this many seconds before exiting (default 0).\n");
}

static void parseCSV(const char* arg, int* values, int* size)
{
    char* token;
    char buffer[256];
    int index = 0;

    strncpy(buffer, arg, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    token = strtok(buffer, ",");
    while (token != NULL && index < ARRAY_SIZE) {
        values[index++] = atoi(token);
        token = strtok(NULL, ",");
    }

    *size = index;
}

typedef struct {
    char clientName[256];
    int ack;
    int delay[ARRAY_SIZE];
    int delaySize;
    uint32_t clientId;
    volatile int transactionId;
    pthread_t asyncThread;
} Controller;

static void* asyncThreadMain(void* arg)
{
    Controller* controller = (Controller*)arg;
    usleep((controller->ack - 0.05) * 1000000); // Ack delay minus 50ms
    PowerController_PowerModePreChangeComplete(controller->clientId, controller->transactionId);
    return NULL;
}

static void onPowerModePreChangeEvent(const PowerController_PowerState_t currentState,
                                      const PowerController_PowerState_t newState,
                                      const int transactionId, const int stateChangeAfter, void* userdata)
{
    Controller* controller = (Controller*)userdata;

    printf("onPowerModePreChangeEvent currentState: %d, newState: %d, clientId: %d, transactionId: %d, stateChangeAfter: %d\n",
           currentState, newState, controller->clientId, transactionId, stateChangeAfter);

    controller->transactionId = transactionId;

    if (controller->ack == 0) {
        PowerController_PowerModePreChangeComplete(controller->clientId, transactionId);
    } else if (controller->ack > 0) {
        pthread_create(&(controller->asyncThread), NULL, asyncThreadMain, controller);
    }
}

static void initController(Controller* controller)
{
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

    PowerController_RegisterPowerModePreChangeCallback(onPowerModePreChangeEvent, controller);

    if (strlen(controller->clientName) > 0) {
        PowerController_AddPowerModePreChangeClient(controller->clientName, &(controller->clientId));
    }
}

static void terminateController(Controller* controller)
{
    if (controller->asyncThread) {
        pthread_join(controller->asyncThread, NULL);
    }

    if (strlen(controller->clientName) > 0) {
        PowerController_RemovePowerModePreChangeClient(controller->clientId);
    }

    PowerController_UnRegisterPowerModePreChangeCallback(onPowerModePreChangeEvent);

    PowerController_Term();
}

static void runDelay(Controller* controller)
{
    int retry = 5;

    // Wait for PowerManager to respond
    // `transactionId` will be updated on receiving onPowerModePreChangeEvent
    while (controller->transactionId == 0 && retry-- > 0) {
        usleep(100 * 1000); // 100ms
    }

    if (controller->transactionId > 0) {
        for (int i = 0; i < controller->delaySize; i++) {
            if (controller->delay[i] > 0) {
                PowerController_DelayPowerModeChangeBy(controller->clientId, controller->transactionId, controller->delay[i]);
                usleep((controller->delay[i] - 0.05) * 1000000); // Delay minus 50ms
            }
        }
    }
}

/**
 * Test application to check whether the box is in standby or not.
 * This has been developed to resolve, XONE-4598
 */
int main(int argc, char* argv[])
{
    PowerController_PowerState_t powerstate = POWER_STATE_ON;
    struct option longOptions[] = {
        { "client", required_argument, NULL, 'c' },
        { "ack", required_argument, NULL, 'a' },
        { "delay", required_argument, NULL, 'd' },
        { "await", required_argument, NULL, 'w' },
        { NULL, 0, NULL, 0 }
    };

    Controller controller = { .clientName = "", .ack = -1, .delaySize = 0, .clientId = 0, .transactionId = 0 };

    int await = 0;
    char argstate[256] = "";

    int opt;
    int optionIndex = 0;
    while ((opt = getopt_long(argc, argv, "c:a:d:w:", longOptions, &optionIndex)) != -1) {
        switch (opt) {
        case 'c':
            strncpy(controller.clientName, optarg, sizeof(controller.clientName) - 1);
            break;
        case 'a':
            controller.ack = atoi(optarg);
            break;
        case 'd':
            parseCSV(optarg, controller.delay, &controller.delaySize);
            break;
        case 'w':
            await = atoi(optarg);
            break;
        default:
            usage();
            return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        strncpy(argstate, argv[optind], sizeof(argstate) - 1);
    } else {
        fprintf(stderr, "Error: POWER_STATE is mandatory.\n");
        usage();
        return EXIT_FAILURE;
    }

    if (strncasecmp(pPowerON, argstate, strlen(pPowerON)) == 0) {
        powerstate = POWER_STATE_ON;
        printf("ON Request...\n");
    } else if (strncasecmp(pPowerStandby, argstate, strlen(pPowerStandby)) == 0) {
        powerstate = POWER_STATE_STANDBY;
        printf("STANDBY Request...\n");
    } else if (strncasecmp(pPowerLigtSleep, argstate, strlen(pPowerLigtSleep)) == 0) {
        powerstate = POWER_STATE_STANDBY_LIGHT_SLEEP;
        printf("Light Sleep Request...\n");
    } else if (strncasecmp(pPowerDeepSleep, argstate, strlen(pPowerDeepSleep)) == 0) {
        powerstate = POWER_STATE_STANDBY_DEEP_SLEEP;
        printf("Deep Sleep Request...\n");
    } else if (strncasecmp(pPowerOFF, argstate, strlen(pPowerOFF)) == 0) {
        powerstate = POWER_STATE_OFF;
        printf("OFF Request...\n");
    } else if (strcmp(argstate, pNOP) == 0) {
        powerstate = POWER_STATE_UNKNOWN;
        printf("NOP Request...\n");
    } else {
        usage();
        return EXIT_FAILURE;
    }

    initController(&controller);

    // Wait for parallel `SetPowerState` run
    usleep(100 * 1000);

    if (POWER_STATE_UNKNOWN != powerstate) {
        int keyCode = 0;
        uint32_t res = PowerController_SetPowerState(keyCode, powerstate, "sys_mon_tool[SetPowerState]");
        if (POWER_CONTROLLER_ERROR_NONE == res) {
            printf("SetPowerState :: Success \n");
        } else if (POWER_CONTROLLER_ERROR_UNAVAILABLE == res) {
            printf("SetPowerState :: Failed :: PowerManager plugin unavailable\n");
        } else {
            printf("SetPowerState :: Failed \n");
        }
    }

    runDelay(&controller);

    if (await > 0) {
        sleep(await);
    }

    terminateController(&controller);

    return 0;
}
