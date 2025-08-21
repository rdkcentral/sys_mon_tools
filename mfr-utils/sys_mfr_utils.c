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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mfrTypes.h"
#include "libIARMCore.h"
#include "mfrMgr.h"
#include "libIBus.h"
#include "mfrApi.h"

#if defined(YOCTO_BUILD)
    const char* validParams[] = {"--CurrentImageFilename", "--FlashedFilename", "--Modelname", "--HardwareId", "--Manufacturer", "--MfgSerialnumber", "--PDRIVersion"};
    const int numberOfParams = 7;
#else
    const char* validParams[] = {"--CurrentImageFilename", "--FlashedFilename", "--Modelname", "--HardwareId", "--Manufacturer", "--MfgSerialnumber"};
    const int numberOfParams = 6;
#endif

/* both the args CurrentImageFilename & FlashedFilename are querying mfrSERIALIZED_TYPE_IMAGENAME.
 * Leaving it as it is, to avoid breaking the caller */
const mfrSerializedType_t mfr_args[] = {mfrSERIALIZED_TYPE_IMAGENAME, mfrSERIALIZED_TYPE_IMAGENAME, mfrSERIALIZED_TYPE_MODELNAME, mfrSERIALIZED_TYPE_HWID, mfrSERIALIZED_TYPE_MANUFACTURER, mfrSERIALIZED_TYPE_MANUFACTURING_SERIALNUMBER, mfrSERIALIZED_TYPE_PDRIVERSION};
const char* mfr_args_str[] = {"mfrSERIALIZED_TYPE_IMAGENAME", "mfrSERIALIZED_TYPE_IMAGENAME", "mfrSERIALIZED_TYPE_MODELNAME", "mfrSERIALIZED_TYPE_HWID", "mfrSERIALIZED_TYPE_MANUFACTURER", "mfrSERIALIZED_TYPE_MANUFACTURING_SERIALNUMBER", "mfrSERIALIZED_TYPE_PDRIVERSION"}; 


void displayHelp() {
     printf("Usage : mfr_util [CMD] \n");
     printf("CMDs are \n" );
     printf("%5s -> %s \n","--help", "print this help.");
     printf("%5s -> %s \n","--CurrentImageFilename", "Get current running imagename ");
     printf("%5s -> %s \n","--FlashedFilename", "Get current flashed imagename ");
     printf("%5s -> %s \n","--Modelname", "Get Model name");
     printf("%5s -> %s \n","--HardwareId", "Get Hardware ID");
     printf("%5s -> %s \n","--Manufacturer", "Get Manufacturer name");
     printf("%5s -> %s \n","--MfgSerialnumber", "Get Manufacturer serial number");
#if defined(YOCTO_BUILD)
     printf("%5s -> %s \n","--PDRIVersion", "Get current PDRIVersion ");
#endif

}

/**
   Return the index of parameter to be retrived if valid.
   If not valid return -1 and display the help screen
**/
int validateParams(const char* param) {
    int paramIndex = -1 ;
    int i = 0 ;
    for ( i=0; i < numberOfParams; i++ ) {
        if (strcmp(param, validParams[i]) == 0 ) {
            paramIndex = i ;
            break ;
        }
    }
    return paramIndex;
}

int main(int argc, char *argv[])
{

    int paramIndex = 0;

    if (argc != 2) {
        displayHelp();
        return -1;
    }

    paramIndex = validateParams(argv[1]);
    if( paramIndex == -1 ){
        displayHelp();
        return -1;
    }

    IARM_Bus_MFRLib_GetSerializedData_Param_t *param;
    IARM_Result_t ret;

    //redirect stdout to null to avoid printing debug prints from IARM Bus
    int fp_old = dup(1);  // preserve the original stdout
    if(fp_old == -1) {
        printf("dup() failed to preserve stdout\n");
        return -1;
    }
    if(freopen ("/dev/null", "w", stdout) == NULL){
        printf("freopen() failed to redirect stdout\n");
        close(fp_old);
        return -1;
    }

    IARM_Bus_Init("mfr_util");
    IARM_Bus_Connect();
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);

    fflush(stdout); // ensure buffer is flushed
    // restore original stdout
    if (dup2(fp_old, fileno(stdout)) == -1) {
        printf("dup2() failed to restore stdout\n");
        close(fp_old);
        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
        return -1;
    }
    close(fp_old);

    param->type = mfr_args[paramIndex];;

    ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
              IARM_BUS_MFRLIB_API_GetSerializedData,
              (void *)param,
              sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

    if(ret != IARM_RESULT_SUCCESS)
    {
       printf("Call failed for %s: error code:%d\n", mfr_args_str[paramIndex], ret);
    }
    else
    {
       int len = param->bufLen + 1;
       char *pTmpStr = (char *)malloc(len);
       memset(pTmpStr,0,len);
       memcpy(pTmpStr,param->buffer,param->bufLen);
       printf("%s\n", pTmpStr);
       free(pTmpStr);
    }
    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);

    fp_old = dup(1);  // preserve the original stdout
    if(fp_old == -1) {
        printf("dup() failed to preserve stdout\n");
        return -1;
    }
    if(freopen ("/dev/null", "w", stdout) == NULL){
        printf("freopen() failed to redirect stdout\n");
        close(fp_old);
        return -1;
    }

    IARM_Bus_Disconnect();
    IARM_Bus_Term();

    fflush(stdout); // ensure buffer is flushed
    // restore original stdout
    if (dup2(fp_old, fileno(stdout)) == -1) {
        printf("dup2() failed to restore stdout\n");
        close(fp_old);
        return -1;
    }
    close(fp_old);

    return 0;
}

