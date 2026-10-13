#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define main iarm_event_sender_main
#include "../iarm-event-sender/IARM_event_sender.c"
#undef main

#define MAX_CAPTURED_EVENT_SIZE 4096

static int assertion_failures;
static int bus_init_calls;
static int bus_connect_calls;
static int bus_disconnect_calls;
static int bus_term_calls;
static int broadcast_calls;
static IARM_Result_t broadcast_result = IARM_RESULT_SUCCESS;
static IARM_EventId_t captured_event_id;
static size_t captured_event_size;
static char captured_owner[128];
static unsigned char captured_event[MAX_CAPTURED_EVENT_SIZE];

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        assertion_failures++; \
    } \
} while (0)

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

IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId,
                                     void *argument, size_t argumentLength)
{
    broadcast_calls++;
    captured_event_id = eventId;
    captured_event_size = argumentLength;
    snprintf(captured_owner, sizeof(captured_owner), "%s", ownerName);
    if (argumentLength <= sizeof(captured_event)) {
        memcpy(captured_event, argument, argumentLength);
    }
    return broadcast_result;
}

static void reset_mocks(void)
{
    bus_init_calls = 0;
    bus_connect_calls = 0;
    bus_disconnect_calls = 0;
    bus_term_calls = 0;
    broadcast_calls = 0;
    broadcast_result = IARM_RESULT_SUCCESS;
    captured_event_id = 0;
    captured_event_size = 0;
    captured_owner[0] = '\0';
    memset(captured_event, 0, sizeof(captured_event));
}

static void test_parse_argument_types(void)
{
    ArgValue value;

    CHECK(parseArg("-17", ARG_INT, &value));
    CHECK(value.type == ARG_INT);
    CHECK(value.val.i == -17);
    CHECK(parseArg("TRUE", ARG_BOOL, &value));
    CHECK(value.val.b);
    CHECK(parseArg("false", ARG_BOOL, &value));
    CHECK(!value.val.b);
    CHECK(!parseArg("yes", ARG_BOOL, &value));
    CHECK(parseArg("language", ARG_STRING, &value));
    CHECK(strcmp(value.val.s, "language") == 0);
}

static void test_system_state_event_payload(void)
{
    GString *name = g_string_new("GatewayConnEvent");
    IARM_Bus_SYSMgr_EventData_t *event_data;

    reset_mocks();
    CHECK(sendIARMEvent(name, 1) == IARM_RESULT_SUCCESS);
    CHECK(broadcast_calls == 1);
    CHECK(strcmp(captured_owner, IARM_BUS_SYSMGR_NAME) == 0);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE);
    CHECK(captured_event_size == sizeof(IARM_Bus_SYSMgr_EventData_t));
    event_data = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(event_data->data.systemStates.stateId == IARM_BUS_SYSMGR_SYSSTATE_GATEWAY_CONNECTION);
    CHECK(event_data->data.systemStates.state == 1);
    CHECK(event_data->data.systemStates.error == 0);
    CHECK(bus_init_calls == 1);
    CHECK(bus_connect_calls == 1);
    CHECK(bus_disconnect_calls == 1);
    CHECK(bus_term_calls == 1);
    g_string_free(name, TRUE);
}

static void test_custom_state_and_intrusion_payloads(void)
{
    GString *name;
    IARM_Bus_SYSMgr_EventData_t *state_event;
    IARM_Bus_SYSMgr_IntrusionData_t *intrusion_event;
    char payload[] = "test intrusion payload";

    reset_mocks();
    CHECK(sendCustomIARMEvent(31, 2, 8) == IARM_RESULT_SUCCESS);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE);
    state_event = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(state_event->data.systemStates.stateId == 31);
    CHECK(state_event->data.systemStates.state == 2);
    CHECK(state_event->data.systemStates.error == 8);

    reset_mocks();
    name = g_string_new("IntrusionEvent");
    CHECK(sendIARMEventPayload(name, payload) == IARM_RESULT_SUCCESS);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_INTRUSION);
    intrusion_event = (IARM_Bus_SYSMgr_IntrusionData_t *)captured_event;
    CHECK(strcmp(intrusion_event->intrusionData, payload) == 0);
    g_string_free(name, TRUE);
}

static void test_dsmgr_event_dispatch_and_invalid_arguments(void)
{
    char *valid_argv[] = { "IARM_event_sender", "DSMgr_EventRxSense", "23", NULL };
    char *invalid_argv[] = { "IARM_event_sender", "DSMgr_HdmiHotPlug", "maybe", NULL };
    char *wrong_count_argv[] = { "IARM_event_sender", "DSMgr_EventRxSense", NULL };
    char *unknown_event_argv[] = { "IARM_event_sender", "DSMgr_Unknown", NULL };
    IARM_Bus_DSMgr_EventData_t *event_data;

    reset_mocks();
    handleIARMEvents(3, valid_argv);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_DSMGR_EVENT_RX_SENSE);
    event_data = (IARM_Bus_DSMgr_EventData_t *)captured_event;
    CHECK(event_data->data.hdmi_rxsense.status == 23);
    CHECK(bus_init_calls == 1);
    CHECK(bus_disconnect_calls == 1);

    reset_mocks();
    handleIARMEvents(3, invalid_argv);
    CHECK(broadcast_calls == 0);
    CHECK(bus_init_calls == 0);

    reset_mocks();
    handleIARMEvents(2, wrong_count_argv);
    CHECK(broadcast_calls == 0);
    CHECK(bus_init_calls == 0);

    reset_mocks();
    handleIARMEvents(2, unknown_event_argv);
    CHECK(broadcast_calls == 0);
    CHECK(bus_init_calls == 0);
}

static void test_all_registered_dsmgr_handlers(void)
{
    size_t event_index;

    for (event_index = 0; event_index < (size_t)iarmEventTableSize; event_index++) {
        char *argv[MAX_ARG_NO_TYPE + 3] = { "IARM_event_sender", NULL };
        char values[MAX_ARG_NO_TYPE][16];
        IARM_EventEntry *entry = &iarmEventTable[event_index];
        int argument_index;

        argv[1] = (char *)entry->name;
        for (argument_index = 0; argument_index < entry->argc_expected; argument_index++) {
            switch (entry->arg_types[argument_index]) {
                case ARG_INT:
                    snprintf(values[argument_index], sizeof(values[argument_index]), "%d", argument_index + 1);
                    break;
                case ARG_BOOL:
                    snprintf(values[argument_index], sizeof(values[argument_index]), "true");
                    break;
                case ARG_STRING:
                    snprintf(values[argument_index], sizeof(values[argument_index]), "sample");
                    break;
            }
            argv[argument_index + 2] = values[argument_index];
        }

        reset_mocks();
        handleIARMEvents(entry->argc_expected + 2, argv);
        CHECK(broadcast_calls == 1);
        CHECK(bus_init_calls == 1);
        CHECK(bus_connect_calls == 1);
        CHECK(bus_disconnect_calls == 1);
        CHECK(bus_term_calls == 1);
    }
}

static void test_broadcast_error_is_propagated(void)
{
    GString *name = g_string_new("NTPReceivedEvent");

    reset_mocks();
    broadcast_result = IARM_RESULT_INVALID_PARAM;
    CHECK(sendIARMEvent(name, 5) == IARM_RESULT_INVALID_PARAM);
    CHECK(broadcast_calls == 1);
    g_string_free(name, TRUE);
}

static void test_cli_usage_and_invalid_arguments(void)
{
    char *missing_argv[] = { "IARM_event_sender", NULL };
    char *invalid_argv[] = { "IARM_event_sender", "unknown", NULL };

    reset_mocks();
    CHECK(iarm_event_sender_main(1, missing_argv) == 1);
    CHECK(broadcast_calls == 0);

    reset_mocks();
    CHECK(iarm_event_sender_main(2, invalid_argv) == 1);
    CHECK(broadcast_calls == 0);
}

static void test_cli_routes_system_custom_and_dsmgr_events(void)
{
    char *system_state_argv[] = { "IARM_event_sender", "GatewayConnEvent", "1", NULL };
    char *filter_argv[] = { "IARM_event_sender", "EISSFilterEvent", "5", NULL };
    char *custom_argv[] = { "IARM_event_sender", "CustomEvent", "41", "2", "9", NULL };
    IARM_Bus_SYSMgr_EventData_t *system_event;

    reset_mocks();
    CHECK(iarm_event_sender_main(3, system_state_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE);
    system_event = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(system_event->data.systemStates.stateId == IARM_BUS_SYSMGR_SYSSTATE_GATEWAY_CONNECTION);
    CHECK(system_event->data.systemStates.state == 1);

    reset_mocks();
    CHECK(iarm_event_sender_main(3, filter_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_EISS_FILTER_STATUS);
    system_event = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(system_event->data.eissEventData.filterStatus == 5);

    reset_mocks();
    CHECK(iarm_event_sender_main(5, custom_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE);
    system_event = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(system_event->data.systemStates.stateId == 41);
    CHECK(system_event->data.systemStates.state == 2);
    CHECK(system_event->data.systemStates.error == 9);

}

static void test_cli_intrusion_payload_and_truncation(void)
{
    char payload[] = "intrusion payload from cli";
    char long_payload[IARM_BUS_SYSMGR_Intrusion_MaxLen + 64];
    char *normal_argv[] = { "IARM_event_sender", "IntrusionEvent", "unused", payload, NULL };
    char *long_argv[] = { "IARM_event_sender", "IntrusionEvent", "unused", long_payload, NULL };
    IARM_Bus_SYSMgr_IntrusionData_t *event_data;

    reset_mocks();
    CHECK(iarm_event_sender_main(4, normal_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_INTRUSION);
    event_data = (IARM_Bus_SYSMgr_IntrusionData_t *)captured_event;
    CHECK(strcmp(event_data->intrusionData, payload) == 0);

    memset(long_payload, 'x', sizeof(long_payload) - 1);
    long_payload[sizeof(long_payload) - 1] = '\0';
    reset_mocks();
    CHECK(iarm_event_sender_main(4, long_argv) == 0);
    CHECK(broadcast_calls == 1);
    event_data = (IARM_Bus_SYSMgr_IntrusionData_t *)captured_event;
    CHECK(strchr(event_data->intrusionData, INTRU_ABREV) != NULL);
    CHECK(strstr(event_data->intrusionData, JSON_TERM) != NULL);
}

static void test_cli_usb_mount_and_detection_events(void)
{
    char *mount_argv[] = { "IARM_event_sender", "USBMountChangedEvent", "1", "/dev/sda1", "/media/usb", NULL };
    char *detect_argv[] = { "IARM_event_sender", "usbdetected", "add", "vendor", "product", "device", NULL };
    IARM_Bus_SYSMgr_EventData_t *event_data;

    reset_mocks();
    CHECK(iarm_event_sender_main(5, mount_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_USB_MOUNT_CHANGED);
    event_data = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(event_data->data.usbMountData.mounted == 1);
    CHECK(strcmp(event_data->data.usbMountData.device, "/dev/sda1") == 0);
    CHECK(strcmp(event_data->data.usbMountData.dir, "/media/usb") == 0);

    reset_mocks();
    CHECK(iarm_event_sender_main(6, detect_argv) == 0);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_SYSSTATE_USB_DETECTED);
    event_data = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(event_data->data.usbData.inserted == 0);
    CHECK(strcmp(event_data->data.usbData.vendor, "vendor") == 0);
    CHECK(strcmp(event_data->data.usbData.productid, "product") == 0);
    CHECK(strcmp(event_data->data.usbData.devicename, "device") == 0);
}

static void test_eiss_app_id_payload_dispatch(void)
{
    GString *event_name = g_string_new("EISSAppIdEvent");
    char payload[] = "0123456789abcdefghijklmn";
    IARM_Bus_SYSMgr_EventData_t *event_data;

    reset_mocks();
    CHECK(sendIARMEventPayload(event_name, payload) == IARM_RESULT_SUCCESS);
    CHECK(broadcast_calls == 1);
    CHECK(captured_event_id == IARM_BUS_SYSMGR_EVENT_EISS_APP_ID_UPDATE);
    event_data = (IARM_Bus_SYSMgr_EventData_t *)captured_event;
    CHECK(event_data->data.eissAppIDList.count == 4);
    CHECK(memcmp(event_data->data.eissAppIDList.idList, payload,
                 sizeof(event_data->data.eissAppIDList.idList)) == 0);
    g_string_free(event_name, TRUE);
}

static void test_cli_peripheral_payload_route(void)
{
    char *peripheral_argv[] = { "IARM_event_sender", "PeripheralUpgradeEvent", "/firmware", "image.bin", NULL };

    reset_mocks();
    CHECK(iarm_event_sender_main(4, peripheral_argv) == 0);
    CHECK(broadcast_calls == 0);
    CHECK(bus_init_calls == 1);
    CHECK(bus_disconnect_calls == 1);
    CHECK(bus_term_calls == 1);
}

int main(void)
{
    test_parse_argument_types();
    test_system_state_event_payload();
    test_custom_state_and_intrusion_payloads();
    test_dsmgr_event_dispatch_and_invalid_arguments();
    test_all_registered_dsmgr_handlers();
    test_broadcast_error_is_propagated();
    test_cli_usage_and_invalid_arguments();
    test_cli_routes_system_custom_and_dsmgr_events();
    test_cli_intrusion_payload_and_truncation();
    test_cli_usb_mount_and_detection_events();
    test_eiss_app_id_payload_dispatch();
    test_cli_peripheral_payload_route();

    if (assertion_failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", assertion_failures);
        return 1;
    }
    puts("IARM event sender L1 tests passed");
    return 0;
}