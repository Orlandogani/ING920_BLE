#include <stddef.h>
#include "platform_api.h"
#include "att_db.h"
#include "gap.h"
#include "btstack_event.h"
#include "btstack_defines.h"
#include "../xb1_data_lib.h"

// GATT characteristic handles
#include "../data/gatt.const"

#define XBOX_ADC_CH_LX   0
#define XBOX_ADC_CH_LY   1
#define XBOX_ADC_CH_RX   2
#define XBOX_ADC_CH_RY   3
#define XBOX_ADC_CH_LT   4
#define XBOX_ADC_CH_RT   5

// Gamepad report rate. platform_set_timer unit is 625us, so 16 units ~= 10ms (~100 Hz).
#define REPORT_INTERVAL_UNITS   160

const static uint8_t adv_data[] = {
    #include "../data/advertising.adv"
};

const static uint8_t scan_data[] = {
    #include "../data/scan_response.adv"
};

const static uint8_t profile_data[] = {
    #include "../data/gatt.profile"
};

static uint32_t total_bytes = 0;
static hci_con_handle_t handle_send = 0;
static uint8_t notify_enable = 0;
static XBOX_ONE_DATA_PACKET_T polling_report;

static void report_timer_callback(void);

static uint16_t scale_adc_to_u16(uint16_t adc_value)
{
    return map(adc_value, 0, ADC_MAX_VALUE, 0, UINT16_MAX);
}

static void build_polling_report(XBOX_ONE_DATA_PACKET_T *report)
{
    report->digital_key.button = 0;
    report->digital_key.Alive = 1;

    report->u16_joystick[JOYSTICK_LX] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_LX));
    report->u16_joystick[JOYSTICK_LY] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_LY));
    report->u16_joystick[JOYSTICK_RX] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_RX));
    report->u16_joystick[JOYSTICK_RY] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_RY));
    report->u16_trigger[TRIGGER_LT] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_LT));
    report->u16_trigger[TRIGGER_RT] = scale_adc_to_u16(ADC_DRV_ReadChannel(XBOX_ADC_CH_RT));
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset,
                                  uint8_t * buffer, uint16_t buffer_size)
{
    switch (att_handle)
    {
    case HANDLE_GENERIC_INPUT:
        if (buffer != NULL)
        {
            *(uint32_t *)buffer = total_bytes;
            return buffer_size;
        }
        else
            return sizeof(total_bytes);
    default:
        return 0;
    }
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode,
                              uint16_t offset, const uint8_t *buffer, uint16_t buffer_size)
{
    switch (att_handle)
    {
    case HANDLE_GENERIC_INPUT:
        total_bytes += buffer_size;
        platform_printf("RX %d bytes (total %u):", buffer_size, total_bytes);
        for (uint16_t i = 0; i < buffer_size; i++)
            platform_printf(" %02X", buffer[i]);
        platform_printf("\n");

        if (notify_enable)
            att_server_notify(handle_send, HANDLE_GENERIC_OUTPUT, (uint8_t *)buffer, buffer_size);
        return 0;
    case HANDLE_GENERIC_OUTPUT + 1:
        if(*(uint16_t *)buffer == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION)
        {
            notify_enable = 1;
            platform_printf("Notifications enabled\n");
            platform_set_timer(report_timer_callback, REPORT_INTERVAL_UNITS);
        }
        else 
        {
            platform_printf("Notifications disabled\n");
            notify_enable = 0;
        }
        return 0;
    default:
        return 0;
    }
}

static void user_msg_handler(uint32_t msg_id, void *data, uint16_t size)
{
    switch (msg_id)
    {
        // add your code
    //case MY_MESSAGE_ID:
    //    break;
    default:
        ;
    }
}

void send_data(void)
{
    if (!notify_enable){
        platform_printf("Notifications not enabled, skip sending\n");
        return;
    }

    if(!att_server_can_send_packet_now(handle_send)){
        platform_printf("Cannot send notification now, skip sending\n");
        return;
    }

    build_polling_report(&polling_report);
    platform_printf("TX %d bytes:", sizeof(polling_report));
    for (size_t i = 0; i < sizeof(polling_report); i++)
        platform_printf(" %02X", ((uint8_t *)&polling_report)[i]);
    platform_printf("\n");
    att_server_notify(handle_send, HANDLE_GENERIC_OUTPUT, (uint8_t *)&polling_report,
                    sizeof(polling_report));
}

// Periodic tick that paces the gamepad reports. Runs in task context, so calling
// BTstack APIs here is safe. Re-arms itself only while notifications are enabled,
// so it self-stops on disable/disconnect. One can-send-now request is outstanding
// at a time, leaving the link free to service incoming writes between reports.
static void report_timer_callback(void)
{
    if (!notify_enable)
        return;

    att_server_request_can_send_now_event(handle_send);
    platform_set_timer(report_timer_callback, REPORT_INTERVAL_UNITS);
}

static void hint_ce_len(uint16_t interval)
{
    uint16_t ce_len = interval << 1;
    if (ce_len > 20)
        ll_hint_on_ce_len(0, ce_len - 15, ce_len - 15);
}

static void user_packet_handler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
{
    const static ext_adv_set_en_t adv_sets_en[] = {{.handle = 0, .duration = 0, .max_events = 0}};
    bd_addr_t rand_addr = {1,2,3,4,5,7};    // TODO: random address generation
    uint8_t event = hci_event_packet_get_type(packet);
    const btstack_user_msg_t *p_user_msg;
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (event)
    {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
            break;
        gap_set_adv_set_random_addr(0, rand_addr);

#ifdef LONG_RANGE
        gap_set_ext_adv_para(0,
                                CONNECTABLE_ADV_BIT,
                                0x00a1, 0x00a1,            // Primary_Advertising_Interval_Min, Primary_Advertising_Interval_Max
                                PRIMARY_ADV_ALL_CHANNELS,  // Primary_Advertising_Channel_Map
                                BD_ADDR_TYPE_LE_RANDOM,    // Own_Address_Type
                                BD_ADDR_TYPE_LE_PUBLIC,    // Peer_Address_Type (ignore)
                                NULL,                      // Peer_Address      (ignore)
                                ADV_FILTER_ALLOW_ALL,      // Advertising_Filter_Policy
                                127,                       // Advertising_Tx_Power
                                PHY_CODED,                 // Primary_Advertising_PHY
                                0,                         // Secondary_Advertising_Max_Skip
                                PHY_CODED,                 // Secondary_Advertising_PHY
                                0x00,                      // Advertising_SID
                                0x00);                     // Scan_Request_Notification_Enable
#else
        gap_set_ext_adv_para(0,
                                CONNECTABLE_ADV_BIT | SCANNABLE_ADV_BIT | LEGACY_PDU_BIT,
                                0x00a1, 0x00a1,            // Primary_Advertising_Interval_Min, Primary_Advertising_Interval_Max
                                PRIMARY_ADV_ALL_CHANNELS,  // Primary_Advertising_Channel_Map
                                BD_ADDR_TYPE_LE_RANDOM,    // Own_Address_Type
                                BD_ADDR_TYPE_LE_PUBLIC,    // Peer_Address_Type (ignore)
                                NULL,                      // Peer_Address      (ignore)
                                ADV_FILTER_ALLOW_ALL,      // Advertising_Filter_Policy
                                0x00,                      // Advertising_Tx_Power
                                PHY_1M,                    // Primary_Advertising_PHY
                                0,                         // Secondary_Advertising_Max_Skip
                                PHY_1M,                    // Secondary_Advertising_PHY
                                0x00,                      // Advertising_SID
                                0x00);                     // Scan_Request_Notification_Enable
#endif
        gap_set_ext_adv_data(0, sizeof(adv_data), (uint8_t*)adv_data);
        gap_set_ext_scan_response_data(0, sizeof(scan_data), (uint8_t*)scan_data);
        gap_set_ext_adv_enable(1, sizeof(adv_sets_en) / sizeof(adv_sets_en[0]), adv_sets_en);
        break;

    case HCI_EVENT_LE_META:
        switch (hci_event_le_meta_get_subevent_code(packet))
        {
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE:
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V2:
            {
                const le_meta_event_enh_create_conn_complete_t *cmpl =
                    decode_hci_le_meta_event(packet, le_meta_event_enh_create_conn_complete_t);
                handle_send = cmpl->handle;
                hint_ce_len(cmpl->interval);
                att_set_db(handle_send, profile_data);
            }
            break;
        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
            hint_ce_len(decode_hci_le_meta_event(packet, le_meta_event_conn_update_complete_t)->interval);
            break;
        default:
            break;
        }

        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        notify_enable = 0;
        platform_printf("Disconnected HCI Event\n");
        gap_set_ext_adv_enable(1, sizeof(adv_sets_en) / sizeof(adv_sets_en[0]), adv_sets_en);
        break;

    case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        platform_printf("ATT_EVENT_MTU updated: %d\n", att_event_mtu_exchange_complete_get_MTU(packet));
        break;

    case ATT_EVENT_CAN_SEND_NOW:
        send_data();
        break;

    case BTSTACK_EVENT_USER_MSG:
        p_user_msg = hci_event_packet_get_user_msg(packet);
        user_msg_handler(p_user_msg->msg_id, p_user_msg->data, p_user_msg->len);
        break;

    default:
        break;
    }
}

uint32_t setup_profile(void *data, void *user_data)
{
    att_server_init(att_read_callback, att_write_callback);
    hci_event_callback_registration.callback = &user_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(&user_packet_handler);
    return 0;
}
