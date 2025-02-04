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

/**
* @file uInputInternal.h
*
* @brief uInput IR Internal API.
*
* This API defines the operations for key management.
*
* @par Document
* Document reference.
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Abbreviations
* - BE:       ig-Endian.
* - cb:       allback function (suffix).
* - DS:      Device Settings.
* - FPD:     Front-Panel Display.
* - HAL:     Hardware Abstraction Layer.
* - LE:      Little-Endian.
* - LS:      Least Significant.
* - MBZ:     Must be zero.
* - MS:      Most Significant.
* - RDK:     Reference Design Kit.
* - _t:      Type (suffix).
*
* @par Implementation Notes
* -# None
*
*/



/**
* @defgroup uinput
* @{
* @defgroup ir
* @{
**/


#ifndef _UINPUT_INTERNAL_
#define _UINPUT_INTERNAL_
#include "libIARM.h"
#include <string.h>
#include <stdbool.h>


#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

extern int b_rdk_logger_enabled;

#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.KEYSIMULATOR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define LOG(...)              printf(__VA_ARGS__)

#endif



typedef void (* uinput_dispatcher_t) (int keyCode, int keyType, int source);

/**
 * @brief uinput module init.
 *
 * initialize uinput module. If /dev/uinput does not exist, fail;
 *
 * @return Error code if fails.
 */
int UINPUT_init(void);

/**
 * @brief get the dispather that will listen for IARM  IR 
 *
 * @return NULL if uinput is not available.
 */
uinput_dispatcher_t UINPUT_GetDispatcher(void);

/**
 * @brief uinput module term.
 *
 * release uinput module. 
 *
 * @return Error code if fails.
 */
int UINPUT_term(void);

#endif
/* End of doxygen group */
/**
 * @}
 */


/** @} */
/** @} */
