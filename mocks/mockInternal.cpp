/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK
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
**/

//#include <rdk_debug.h>
#include <cstdarg>
#include <map>
#include <string>
#include <cstdint>
#include "libIBus.h"
#include "libIARMCore.h"
#include "power_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

IARM_Result_t IARM_Bus_Init(const char* name)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Connect()
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_IsConnected(const char* memberName, int* isRegistered)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RegisterEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_UnRegisterEventHandler(const char* ownerName, IARM_EventId_t eventId)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RemoveEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Call(const char* ownerName, const char* methodName, void* arg, size_t argLen)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId, void *arg, size_t argLen)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RegisterCall(const char* methodName, IARM_BusCall_t handler)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Call_with_IPCTimeout(const char *ownerName,  const char *methodName, void *arg, size_t argLen, int timeout)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RegisterEvent(IARM_EventId_t maxEventId)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Disconnect(void)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Term(void)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Malloc(IARM_MemType_t type, size_t size, void **ptr)
{
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Free(IARM_MemType_t type, void *alloc)
{
    return IARM_RESULT_SUCCESS;
}

void PowerController_Init()
{

}

void PowerController_Term()
{

}

bool PowerController_IsOperational()
{
    return true;
}

uint32_t PowerController_Connect()
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_GetPowerState(PowerController_PowerState_t* currentState, PowerController_PowerState_t* previousState)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_SetPowerState(const int keyCode, const PowerController_PowerState_t powerstate, const char* reason)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_PowerModePreChangeComplete(const uint32_t clientId, const int transactionId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RegisterPowerModePreChangeCallback(PowerController_PowerModePreChangeCb callback, void* userdata)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_UnRegisterPowerModePreChangeCallback(PowerController_PowerModePreChangeCb callback)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_AddPowerModePreChangeClient(const char* clientName, uint32_t* clientId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RemovePowerModePreChangeClient(const uint32_t clientId)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_DelayPowerModeChangeBy(const uint32_t clientId, const int transactionId, const int delayPeriod)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

#if 0
WDMP_STATUS getRFCParameter(const char *pcCallerID, const char* pcParameterName, RFC_ParamData_t *pstParamData)
{
    return WDMP_SUCCESS;
}

rdk_Error rdk_logger_init(const char* debugConfigFile)
{
    return 0;
}

rdk_Error rdk_logger_deinit()
{
    return 0;
}

rdk_logger_Bool rdk_dbg_enabled(const char *module,
                                rdk_LogLevel level)
{
    return true;
}

void RDK_LOG(rdk_LogLevel level,
             const char *module,
             const char *format,
             ...)
{
    char buffer[32 * 1024];

    std::va_list arguments;

    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, arguments);
    buffer[sizeof(buffer) - 1] = 0;
    va_end(arguments);

    printf("%s\n", buffer);
}
#endif

#ifdef __cplusplus
}
#endif
