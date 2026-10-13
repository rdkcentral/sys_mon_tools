#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define main mfr_util_main
#include "../mfr-utils/sys_mfr_utils.c"
#undef main

static int assertion_failures;
static int bus_init_calls;
static int bus_connect_calls;
static int bus_call_calls;
static int bus_free_calls;
static int bus_disconnect_calls;
static int bus_term_calls;
static mfrSerializedType_t requested_type;
static const char response_value[] = "unit-test-mfr-value";

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

IARM_Result_t IARM_Malloc(IARM_MemType_t type, size_t size, void **ptr)
{
    (void)type;
    *ptr = calloc(1, size);
    return (*ptr == NULL) ? IARM_RESULT_INVALID_PARAM : IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Free(IARM_MemType_t type, void *allocation)
{
    (void)type;
    bus_free_calls++;
    free(allocation);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Init(const char *name)
{
    (void)name;
    bus_init_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Connect(void)
{
    bus_connect_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Call(const char *ownerName, const char *methodName,
                            void *argument, size_t argumentLength)
{
    IARM_Bus_MFRLib_GetSerializedData_Param_t *parameter = argument;

    (void)ownerName;
    (void)methodName;
    CHECK(argumentLength == sizeof(*parameter));
    requested_type = parameter->type;
    parameter->bufLen = sizeof(response_value) - 1;
    memcpy(parameter->buffer, response_value, sizeof(response_value));
    bus_call_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Disconnect(void)
{
    bus_disconnect_calls++;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Term(void)
{
    bus_term_calls++;
    return IARM_RESULT_SUCCESS;
}

static void reset_mocks(void)
{
    bus_init_calls = 0;
    bus_connect_calls = 0;
    bus_call_calls = 0;
    bus_free_calls = 0;
    bus_disconnect_calls = 0;
    bus_term_calls = 0;
    requested_type = (mfrSerializedType_t)0;
}

static int run_mfr_with_output(int argc, char **argv, char *output, size_t outputSize)
{
    FILE *capture;
    int saved_stdout;
    int result;
    size_t output_length;

    fflush(stdout);
    saved_stdout = dup(STDOUT_FILENO);
    capture = tmpfile();
    if (saved_stdout < 0 || capture == NULL) {
        if (saved_stdout >= 0) {
            close(saved_stdout);
        }
        if (capture != NULL) {
            fclose(capture);
        }
        return -1;
    }
    if (dup2(fileno(capture), STDOUT_FILENO) < 0) {
        close(saved_stdout);
        fclose(capture);
        return -1;
    }

    result = mfr_util_main(argc, argv);
    fflush(stdout);
    fseek(capture, 0, SEEK_SET);
    output_length = fread(output, 1, outputSize - 1, capture);
    output[output_length] = '\0';
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    fclose(capture);
    return result;
}

static void test_parameter_validation(void)
{
    CHECK(validateParams("--CurrentImageFilename") == 0);
    CHECK(validateParams("--FlashedFilename") == 1);
    CHECK(validateParams("--Modelname") == 2);
    CHECK(validateParams("--HardwareId") == 3);
    CHECK(validateParams("--Manufacturer") == 4);
    CHECK(validateParams("--MfgSerialnumber") == 5);
#if defined(YOCTO_BUILD)
    CHECK(validateParams("--PDRIVersion") == 6);
#else
    CHECK(validateParams("--PDRIVersion") == -1);
#endif
    CHECK(validateParams("--unknown") == -1);
}

static void test_cli_queries_matching_type_and_prints_result(void)
{
    struct {
        char *option;
        mfrSerializedType_t expected_type;
    } cases[] = {
        { "--CurrentImageFilename", mfrSERIALIZED_TYPE_IMAGENAME },
        { "--FlashedFilename", mfrSERIALIZED_TYPE_IMAGENAME },
        { "--Modelname", mfrSERIALIZED_TYPE_MODELNAME },
        { "--HardwareId", mfrSERIALIZED_TYPE_HWID },
        { "--Manufacturer", mfrSERIALIZED_TYPE_MANUFACTURER },
        { "--MfgSerialnumber", mfrSERIALIZED_TYPE_MANUFACTURING_SERIALNUMBER },
#if defined(YOCTO_BUILD)
        { "--PDRIVersion", mfrSERIALIZED_TYPE_PDRIVERSION },
#endif
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        char *argv[] = { "mfr_util", cases[index].option, NULL };
        char output[256];

        reset_mocks();
        CHECK(run_mfr_with_output(2, argv, output, sizeof(output)) == 0);
        CHECK(strcmp(output, "unit-test-mfr-value\n") == 0);
        CHECK(requested_type == cases[index].expected_type);
        CHECK(bus_init_calls == 1);
        CHECK(bus_connect_calls == 1);
        CHECK(bus_call_calls == 1);
        CHECK(bus_free_calls == 1);
        CHECK(bus_disconnect_calls == 1);
        CHECK(bus_term_calls == 1);
    }
}

static void test_invalid_cli_arguments_do_not_call_bus(void)
{
    char *invalid_argv[] = { "mfr_util", "--unknown", NULL };
    char *missing_argv[] = { "mfr_util", NULL };
    char output[1024];

    reset_mocks();
    CHECK(run_mfr_with_output(2, invalid_argv, output, sizeof(output)) == -1);
    CHECK(strstr(output, "Usage : mfr_util") != NULL);
    CHECK(bus_call_calls == 0);

    reset_mocks();
    CHECK(run_mfr_with_output(1, missing_argv, output, sizeof(output)) == -1);
    CHECK(strstr(output, "Usage : mfr_util") != NULL);
    CHECK(bus_call_calls == 0);
}

int main(void)
{
    test_parameter_validation();
    test_cli_queries_matching_type_and_prints_result();
    test_invalid_cli_arguments_do_not_call_bus();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("mfr_util L1 tests passed");
    return 0;
}