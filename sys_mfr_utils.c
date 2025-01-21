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

#if defined(YOCTO_BUILD)
    #include "mfrTypes.h"
    #include "libIARMCore.h"
    #include "mfrMgr.h"
    #include "libIBus.h"
    #include "mfrApi.h"
#endif

#include "sys_mfr_utils.h"

#define NVRAM_TEST_SIZE     128

#if defined(YOCTO_BUILD)
    const char* validParams[] = {"--CurrentImageFilename", "--FlashedFilename","--PDRIVersion"};
    const int numberOfParams = 3;
#else
    const char* validParams[] = {"--CurrentImageFilename", "--FlashedFilename"};
    const int numberOfParams = 2;
#endif

void displayHelp() {
     printf("Usage : mfr_util [CMD] \n");
     printf("CMDs are \n" );
     printf("%5s -> %s \n","--help", "print this help.");
     printf("%5s -> %s \n","--CurrentImageFilename", "Get current running imagename ");
     printf("%5s -> %s \n","--FlashedFilename", "Get current flashed imagename ");
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

void getCurrentRunningFileName() {
    IARM_Bus_MFRLib_GetSerializedData_Param_t *param;
    IARM_Result_t ret;
    char *pTmpStr;
    int len;

    //redirect stdout to null to avoid printing debug prints from IARM Bus
    FILE * fp_orig = stdout;  //preserve the original stdout
    stdout = fopen("/dev/null","w");

    IARM_Bus_Init("mfr_util");
    IARM_Bus_Connect();
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);

    stdout=fp_orig;  //restore stdout

    param->type = mfrSERIALIZED_TYPE_IMAGENAME;

    ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
               IARM_BUS_MFRLIB_API_GetSerializedData,
           (void *)param,
           sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

    if(ret != IARM_RESULT_SUCCESS)
    {
       printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret);
    }
    else
    {
       len = param->bufLen + 1;
       pTmpStr = (char *)malloc(len);
       memset(pTmpStr,0,len);
       memcpy(pTmpStr,param->buffer,param->bufLen);
       printf("%s\n", pTmpStr);
       free(pTmpStr);
    }
    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
    return ;
}

void getCurrentFlashedFileName() {
    IARM_Bus_MFRLib_GetSerializedData_Param_t *param;
    IARM_Result_t ret;
    char *pTmpStr;
    int len;

    //redirect stdout to null to avoid printing debug prints from IARM Bus
    FILE * fp_orig = stdout;  //preserve the original stdout
    stdout = fopen("/dev/null","w");

    IARM_Bus_Init("mfr_util");
    IARM_Bus_Connect();
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);

    stdout=fp_orig;  //restore stdout

    param->type = mfrSERIALIZED_TYPE_IMAGENAME;

    ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
               IARM_BUS_MFRLIB_API_GetSerializedData,
           (void *)param,
           sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

    if(ret != IARM_RESULT_SUCCESS)
    {
       printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret);
    }
    else
    {
       len = param->bufLen + 1;
       pTmpStr = (char *)malloc(len);
       memset(pTmpStr,0,100*len);
       memcpy(pTmpStr,param->buffer,100*param->bufLen);
       printf("%s\n", pTmpStr);
       free(pTmpStr);
    }
    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
    return ;
}


#if defined(YOCTO_BUILD)
void getPDRIVersion(){

    IARM_Bus_MFRLib_GetSerializedData_Param_t *param;
    IARM_Result_t ret;
    char *pTmpStr;
    int len;

    //redirect stdout to null to avoid printing debug prints from IARM Bus
    FILE * fp_orig = stdout;  //preserve the original stdout
    stdout = fopen("/dev/null","w");

    IARM_Bus_Init("mfr_util");
    IARM_Bus_Connect();
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t), (void**)&param);

    stdout=fp_orig;  //restore stdout

    param->type = mfrSERIALIZED_TYPE_PDRIVERSION;

    ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,
               IARM_BUS_MFRLIB_API_GetSerializedData,
           (void *)param,
           sizeof(IARM_Bus_MFRLib_GetSerializedData_Param_t));

    if(ret != IARM_RESULT_SUCCESS)
    {
       printf("Call failed for %s: error code:%d\n","mfrSERIALIZED_TYPE_PDRIVERSION",ret);
    }
    else
    {
       len = param->bufLen + 1;
       pTmpStr = (char *)malloc(len);
       memset(pTmpStr,0,len);
       memcpy(pTmpStr,param->buffer,param->bufLen);
       printf("%s\n", pTmpStr);
       free(pTmpStr);
    }
    IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,param);
}
#endif

int main(int argc, char *argv[])
{

    int paramIndex = 0;

    if (argc != 2) {
        displayHelp();
        return -1;
    }

    paramIndex = validateParams(argv[1]);

    if( validateParams(argv[1]) == -1 ){
        displayHelp();
        return -1;
    }

    switch(paramIndex) {
        /*Check for validParams array for parameter name mapping*/
        case 0 :
            getCurrentRunningFileName();
            break;
        case 1 :
            getCurrentFlashedFileName();
            break;
#if defined(YOCTO_BUILD)            
        case 2 :
            getPDRIVersion();
            break;
#endif
        default :
            displayHelp();
            break;

    }

    return 0;
}

