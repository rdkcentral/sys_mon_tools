#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "power_controller.h"

#define main set_power_state_main
#include "../iarm_set_powerstate/IARM_BUS_SetPowerStatus.c"
#undef main

static int assertion_failures;
static int init_calls;
static int term_calls;
static int set_state_calls;
static int add_client_calls;
static int remove_client_calls;
static int register_callback_calls;
static int unregister_callback_calls;
static int complete_calls;
static int delay_calls;
static int sleep_calls;
static int usleep_calls;
static int operational = 1;
static int invoke_prechange;
static int set_state_result;
static int last_key_code;
static int last_wait_seconds;
static int last_delay_seconds[ARRAY_SIZE];
static int last_delay_count;
static uint32_t last_client_id;
static int last_transaction_id;
static PowerController_PowerState_t last_power_state;
static PowerController_PowerModePreChangeCb registered_callback;
static void *registered_userdata;
static char last_client_name[256];

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

void PowerController_Init(void)
{
    init_calls++;
}

void PowerController_Term(void)
{
    term_calls++;
}

bool PowerController_IsOperational(void)
{
    return operational != 0;
}

uint32_t PowerController_Connect(void)
{
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_SetPowerState(const int keyCode,
                                       const PowerController_PowerState_t powerstate,
                                       const char *reason)
{
    (void)reason;
    set_state_calls++;
    last_key_code = keyCode;
    last_power_state = powerstate;
    if (invoke_prechange && registered_callback != NULL) {
        registered_callback(POWER_STATE_ON, powerstate, 17, 5, registered_userdata);
    }
    return (uint32_t)set_state_result;
}

uint32_t PowerController_RegisterPowerModePreChangeCallback(
    PowerController_PowerModePreChangeCb callback, void *userdata)
{
    register_callback_calls++;
    registered_callback = callback;
    registered_userdata = userdata;
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_UnRegisterPowerModePreChangeCallback(
    PowerController_PowerModePreChangeCb callback)
{
    unregister_callback_calls++;
    CHECK(callback == registered_callback);
    registered_callback = NULL;
    registered_userdata = NULL;
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_AddPowerModePreChangeClient(const char *clientName,
                                                      uint32_t *clientId)
{
    add_client_calls++;
    snprintf(last_client_name, sizeof(last_client_name), "%s", clientName);
    *clientId = 1234;
    last_client_id = *clientId;
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_RemovePowerModePreChangeClient(const uint32_t clientId)
{
    remove_client_calls++;
    last_client_id = clientId;
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_PowerModePreChangeComplete(const uint32_t clientId,
                                                     const int transactionId)
{
    complete_calls++;
    last_client_id = clientId;
    last_transaction_id = transactionId;
    return POWER_CONTROLLER_ERROR_NONE;
}

uint32_t PowerController_DelayPowerModeChangeBy(const uint32_t clientId,
                                                 const int transactionId,
                                                 const int delayPeriod)
{
    (void)clientId;
    last_transaction_id = transactionId;
    if (last_delay_count < ARRAY_SIZE) {
        last_delay_seconds[last_delay_count++] = delayPeriod;
    }
    delay_calls++;
    return POWER_CONTROLLER_ERROR_NONE;
}

int usleep(useconds_t usec)
{
    (void)usec;
    usleep_calls++;
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    sleep_calls++;
    last_wait_seconds = (int)seconds;
    return 0;
}

static void reset_mocks(void)
{
    init_calls = 0;
    term_calls = 0;
    set_state_calls = 0;
    add_client_calls = 0;
    remove_client_calls = 0;
    register_callback_calls = 0;
    unregister_callback_calls = 0;
    complete_calls = 0;
    delay_calls = 0;
    sleep_calls = 0;
    usleep_calls = 0;
    operational = 1;
    invoke_prechange = 0;
    set_state_result = POWER_CONTROLLER_ERROR_NONE;
    last_key_code = -1;
    last_wait_seconds = 0;
    last_delay_count = 0;
    last_client_id = 0;
    last_transaction_id = 0;
    last_power_state = POWER_STATE_UNKNOWN;
    registered_callback = NULL;
    registered_userdata = NULL;
    last_client_name[0] = '\0';
    memset(last_delay_seconds, 0, sizeof(last_delay_seconds));
    optind = 1;
}

static int run_set_power_state(int argc, char **argv)
{
    reset_mocks();
    return set_power_state_main(argc, argv);
}

static void test_power_state_arguments(void)
{
    struct {
        char *argument;
        PowerController_PowerState_t expected;
    } cases[] = {
        { "ON", POWER_STATE_ON },
        { "STANDBY", POWER_STATE_STANDBY },
        { "LIGHTSLEEP", POWER_STATE_STANDBY_LIGHT_SLEEP },
        { "DEEPSLEEP", POWER_STATE_STANDBY_DEEP_SLEEP },
        { "OFF", POWER_STATE_OFF }
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        char *argv[] = { "SetPowerState", cases[index].argument, NULL };
        CHECK(run_set_power_state(2, argv) == 0);
        CHECK(set_state_calls == 1);
        CHECK(last_power_state == cases[index].expected);
        CHECK(last_key_code == 0);
        CHECK(init_calls == 1);
        CHECK(term_calls == 1);
    }
}

static void test_invalid_and_missing_state_skip_controller(void)
{
    char *invalid_argv[] = { "SetPowerState", "INVALID", NULL };
    char *missing_argv[] = { "SetPowerState", NULL };

    CHECK(run_set_power_state(2, invalid_argv) == EXIT_FAILURE);
    CHECK(init_calls == 0);
    CHECK(run_set_power_state(1, missing_argv) == EXIT_FAILURE);
    CHECK(init_calls == 0);
}

static void test_nop_skips_power_state_call(void)
{
    char *argv[] = { "SetPowerState", "NOP", NULL };

    CHECK(run_set_power_state(2, argv) == 0);
    CHECK(init_calls == 1);
    CHECK(set_state_calls == 0);
    CHECK(term_calls == 1);
}

static void test_client_acknowledgement_and_positive_delays(void)
{
    char *argv[] = {
        "SetPowerState", "--client", "unit-test", "--ack", "0",
        "--delay", "2,0,-1,3", "ON", NULL
    };

    invoke_prechange = 0;
    reset_mocks();
    invoke_prechange = 1;

    CHECK(set_power_state_main(8, argv) == 0);
    CHECK(add_client_calls == 1);
    CHECK(strcmp(last_client_name, "unit-test") == 0);
    CHECK(remove_client_calls == 1);
    CHECK(last_client_id == 1234);
    CHECK(complete_calls == 1);
    CHECK(last_transaction_id == 17);
    CHECK(delay_calls == 2);
    CHECK(last_delay_count == 2);
    CHECK(last_delay_seconds[0] == 2);
    CHECK(last_delay_seconds[1] == 3);
    CHECK(unregister_callback_calls == 1);
}

int main(void)
{
    test_power_state_arguments();
    test_invalid_and_missing_state_skip_controller();
    test_nop_skips_power_state_call();
    test_client_acknowledgement_and_positive_delays();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("SetPowerState L1 tests passed");
    return 0;
}