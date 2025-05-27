/*
 * Copyright 2025 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_controller.h"

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
