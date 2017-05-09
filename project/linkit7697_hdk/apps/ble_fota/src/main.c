/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* device.h includes */
#include "mt7687.h"
#include "system_mt7687.h"

/* wifi related header */
#include "wifi_api.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"

/* ble related header */
#include "bt_init.h"
#include "bt_gatts.h"
#include "bt_gap_le.h"
#include "bt_uuid.h"
#include "bt_debug.h"
#include "bt_type.h"
#include "bt_notify.h"

/* app-wise config */
#include "project_config.h"
#include "sys_init.h"
#include "task_def.h"
#include "flash_map.h"

/* hal */
#include "hal_flash.h"
#include "hal_wdt.h"

#include "fota.h"
#include "nvdm.h"

/* ---------------------------------------------------------------------------- */

/* Create the log control block as user wishes. Here we use 'app' as module name.
 * User needs to define their own log control blocks as project needs.
 * Please refer to the log dev guide under /doc folder for more details.
 */
log_create_module(app, PRINT_LEVEL_INFO);

static void app_fota_init(void);

/* ---------------------------------------------------------------------------- */

#define APP_BLE_FOTA_MAX_INTERVAL          0x00C0    /*The range is from 0x0020 to 0x4000.*/
#define APP_BLE_FOTA_MIN_INTERVAL          0x00C0    /*The range is from 0x0020 to 0x4000.*/
#define APP_BLE_FOTA_CHANNEL_NUM           7
#define APP_BLE_FOTA_FILTER_POLICY         0
#define APP_BLE_FOTA_AD_FLAG_LEN           2
#define APP_BLE_FOTA_AD_UUID_LEN           3

extern bt_bd_addr_t local_public_addr;

// called by bt_app_event_callback@bt_common.c
bt_status_t app_bt_event_callback(bt_msg_type_t msg, bt_status_t status, void *buff)
{
    LOG_I(app, "---> bt_event_callback(0x%08X,%d)", msg, status);

    switch(msg)
    {
    case BT_POWER_ON_CNF:
        LOG_I(app, "[BT_POWER_ON_CNF](%d)", status);

        // set random address before advertising
        LOG_I(app, "bt_gap_le_set_random_address()");    
        bt_gap_le_set_random_address((bt_bd_addr_ptr_t)local_public_addr);

        app_fota_init();
        break;
    case BT_GAP_LE_SET_RANDOM_ADDRESS_CNF: 
        LOG_I(app, "[BT_GAP_LE_SET_RANDOM_ADDRESS_CNF](%d)", status);

        // start advertising
        bt_hci_cmd_le_set_advertising_enable_t enable;
        bt_hci_cmd_le_set_advertising_parameters_t adv_param = {
                .advertising_interval_min = APP_BLE_FOTA_MIN_INTERVAL,
                .advertising_interval_max = APP_BLE_FOTA_MAX_INTERVAL,
                .advertising_type = BT_HCI_ADV_TYPE_CONNECTABLE_UNDIRECTED,
                .own_address_type = BT_ADDR_RANDOM,
                .advertising_channel_map = APP_BLE_FOTA_CHANNEL_NUM,
                .advertising_filter_policy = APP_BLE_FOTA_FILTER_POLICY
            };
        bt_hci_cmd_le_set_advertising_data_t adv_data;

        adv_data.advertising_data[0] = APP_BLE_FOTA_AD_FLAG_LEN;
        adv_data.advertising_data[1] = BT_GAP_LE_AD_TYPE_FLAG;
        adv_data.advertising_data[2] = BT_GAP_LE_AD_FLAG_BR_EDR_NOT_SUPPORTED | BT_GAP_LE_AD_FLAG_GENERAL_DISCOVERABLE;

        adv_data.advertising_data[3] = APP_BLE_FOTA_AD_UUID_LEN;
        adv_data.advertising_data[4] = BT_GAP_LE_AD_TYPE_16_BIT_UUID_COMPLETE;
        adv_data.advertising_data[5] = APP_BLE_FOTA_SERVICE_UUID & 0x00FF;
        adv_data.advertising_data[6] = (APP_BLE_FOTA_SERVICE_UUID & 0xFF00)>>8;

        adv_data.advertising_data[7] = 1+strlen(APP_BLE_FOTA_DEVICE_NAME);
        adv_data.advertising_data[8] = BT_GAP_LE_AD_TYPE_NAME_COMPLETE;
        memcpy(adv_data.advertising_data+9, APP_BLE_FOTA_DEVICE_NAME, strlen(APP_BLE_FOTA_DEVICE_NAME));

        adv_data.advertising_data_length = 9 + strlen(APP_BLE_FOTA_DEVICE_NAME);

        enable.advertising_enable = BT_HCI_ENABLE;
        bt_gap_le_set_advertising(&enable, &adv_param, &adv_data, NULL);
        break;

    case BT_GAP_LE_SET_ADVERTISING_CNF:
        LOG_I(app, "[BT_GAP_LE_SET_ADVERTISING_CNF](%d)", status);
        break;

    case BT_GAP_LE_CONNECT_IND:
        LOG_I(app, "[BT_GAP_LE_CONNECT_IND](%d)", status);

        bt_gap_le_connection_ind_t *connection_ind = (bt_gap_le_connection_ind_t *)buff;
        LOG_I(app, "-> connection handle = 0x%04x, role = %s", connection_ind->connection_handle, (connection_ind->role == BT_ROLE_MASTER)? "master" : "slave");

        LOG_I(app, "************************");
        LOG_I(app, "BLE connected!!");
        LOG_I(app, "************************");
        break;
    }

    LOG_I(app, "<--- bt_event_callback(0x%08X,%d)", msg, status);
    return BT_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------- */

/* ----- begin of do_not_change ----- */

/* command, hardcoded at mobile-side application */
#define FOTA_EXTCMD_UPDATE_BIN_SENDER           "fota_fbin"
#define FOTA_EXTCMD_UPDATE_BIN_RECEIVER         "fota_fbin"

/* fota receive firmware data type */
#define FOTA_FILE_DATA_BEGIN                    0
#define FOTA_FILE_DATA_PACK                     1
#define FOTA_FILE_DATA_END                      2

/* ----- end of do_not_change ----- */

static uint32_t g_total_packet_count = 0;
static uint32_t g_recv_packet;
static uint32_t g_write_address;

static void app_fota_handle_data_start(bt_notify_event_data_t *event_data)
{
    g_total_packet_count = atoi((const char*)event_data->data);
    g_recv_packet = 0;
    g_write_address = FOTA_BASE;
    LOG_I(app, "total packet = %d", g_total_packet_count);
}

#define BLOCK_SIZE  (4096)

static void app_fota_handle_data_packet(bt_notify_event_data_t *event_data)
{
    LOG_I(app, "   >packet recevied/total=%d/%d", g_recv_packet+1, g_total_packet_count);

    hal_flash_status_t result;

    if(event_data->length > BLOCK_SIZE)
    {
        LOG_E(app, "Error packet length = %d > 4k", event_data->length);
        // case not handled
        return;
    }

    // block_addr is the beginning address of block that will be written
    // if black address < writing address, means already erased
    uint32_t block_addr = ((g_write_address+event_data->length)/BLOCK_SIZE)*BLOCK_SIZE;
    if (block_addr >= g_write_address)  // not yet erased
    {
        result = hal_flash_erase(block_addr, HAL_FLASH_BLOCK_4K);
        if(result != HAL_FLASH_STATUS_OK)
        {
            LOG_E(app, "Error erasing flash at address=0x%08X, result=%d", block_addr, result);
            LOG_E(app, "Detail: packet length=%d, recv/total=%d/%d, addr=0x%08X", event_data->length, g_recv_packet+1, g_total_packet_count, g_write_address);
        }
    }

    result = hal_flash_write(g_write_address, event_data->data, event_data->length);
    if (result != HAL_FLASH_STATUS_OK)
    {
        LOG_E(app, "Error writing flash at address=0x%08X, result=%d", g_write_address, result);
        LOG_E(app, "Detail: packet length=%d, recv/total=%d/%d, addr=0x%08X", event_data->length, g_recv_packet+1, g_total_packet_count, g_write_address);
    }

    g_recv_packet ++;
    g_write_address += event_data->length;
}


static void app_fota_btnotify_msg_hdlr(void *data)
{
    bt_notify_callback_data_t *p_data = (bt_notify_callback_data_t *)data;

    LOG_I(app, "> app_fota_btnotify_msg_hdlr(evt_id=%d)", p_data->evt_id);

    switch (p_data->evt_id) 
    {
    case BT_NOTIFY_EVENT_CONNECTION:
        LOG_I(app, "  >BT_NOTIFY_EVENT_CONNECTION");
        break;

    case BT_NOTIFY_EVENT_DISCONNECTION:
        LOG_I(app, "  >BT_NOTIFY_EVENT_DISCONNECTION");
        break;

    case BT_NOTIFY_EVENT_SEND_IND:
        /*send  new/the rest data flow start*/
        LOG_I(app, "  >BT_NOTIFY_EVENT_SEND_IND");
        break;

    case BT_NOTIFY_EVENT_DATA_RECEIVED:
        /*receive data*/
        LOG_I(app, "  >BT_NOTIFY_EVENT_DATA_RECEIVED(code=%d)", p_data->event_data.error_code);
        if (strcmp(p_data->event_data.sender_id, FOTA_EXTCMD_UPDATE_BIN_SENDER) ||
            strcmp(p_data->event_data.receiver_id, FOTA_EXTCMD_UPDATE_BIN_SENDER))
        {
            LOG_E(app, "sender (%s) or receiver (%s) error", p_data->event_data.sender_id, p_data->event_data.receiver_id);
            break;
        }

        switch(p_data->event_data.error_code)
        {
        case FOTA_FILE_DATA_BEGIN:
            app_fota_handle_data_start(&(p_data->event_data));
            break;
        case FOTA_FILE_DATA_PACK:
            app_fota_handle_data_packet(&(p_data->event_data));
            break;
        case FOTA_FILE_DATA_END:
            if(g_recv_packet != g_total_packet_count)
            {
                LOG_E(app, "Packet count mismatch, recv/total=%d/%d", g_recv_packet, g_total_packet_count);
                break;
            }

            {
                // trigger update
                fota_ret_t result;
                result = fota_trigger_update();
                LOG_I(app, "fota_trigger_update() result=%d", result);
            }
            vTaskDelay(2000);
            {
                //reboot device
                hal_wdt_status_t result;
                hal_wdt_config_t wdt_config;
                wdt_config.mode = HAL_WDT_MODE_RESET;
                wdt_config.seconds = 1;
                hal_wdt_init(&wdt_config);
                result = hal_wdt_software_reset();
                LOG_I(app, "hal_wdt_software_reset() result=%d", result);
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    LOG_I(app, "< app_fota_btnotify_msg_hdlr(evt_id=%d)", p_data->evt_id);
}

static void app_fota_init(void)
{
    bt_notify_init(0);
    bt_notify_register_callback(NULL, FOTA_EXTCMD_UPDATE_BIN_SENDER, app_fota_btnotify_msg_hdlr);    

    log_config_print_switch(NOTIFY, DEBUG_LOG_OFF);
    log_config_print_switch(NOTIFY_SRV, DEBUG_LOG_OFF);
    log_config_print_switch(DOGP, DEBUG_LOG_OFF);
    log_config_print_switch(DOGP_ADP, DEBUG_LOG_OFF);
    log_config_print_switch(DOGP_CM, DEBUG_LOG_OFF);
}

/* ---------------------------------------------------------------------------- */

static int32_t _wifi_event_handler(wifi_event_t event,
        uint8_t *payload,
        uint32_t length)
{
    struct netif *sta_if;

    LOG_I(app, "wifi event: %d", event);

    switch(event)
    {
    case WIFI_EVENT_IOT_INIT_COMPLETE:
        LOG_I(app, "wifi inited complete");
        break;
    }

    return 1;
}

/* ---------------------------------------------------------------------------- */

extern void bt_common_init(void);

/**
* @brief       Main function
* @param[in]   None.
* @return      None.
*/
int main(void)
{
    /* Do system initialization, eg: hardware, nvdm and random seed. */
    system_init();

    /* system log initialization.
     * This is the simplest way to initialize system log, that just inputs three NULLs
     * as input arguments. User can use advanved feature of system log along with NVDM.
     * For more details, please refer to the log dev guide under /doc folder or projects
     * under project/mtxxxx_hdk/apps/.
     */
    log_init(NULL, NULL, NULL);

    LOG_I(app, "main()");

    /* Wi-Fi must be initialized for BLE start-up */
    wifi_connection_register_event_handler(WIFI_EVENT_IOT_INIT_COMPLETE , _wifi_event_handler);

    wifi_config_t config = {0};
    config.opmode = WIFI_MODE_STA_ONLY;
    wifi_init(&config, NULL);

    lwip_tcpip_config_t tcpip_config = {{0}, {0}, {0}, {0}, {0}, {0}};
    lwip_tcpip_init(&tcpip_config, WIFI_MODE_STA_ONLY);

    bt_create_task();
    bt_common_init();
	
	/* As for generic HAL init APIs like: hal_uart_init(), hal_gpio_init() and hal_spi_master_init() etc,
     * user can call them when they need, which means user can call them here or in user task at runtime.
     */
    hal_flash_init();
    nvdm_init();

    /* Create a user task for demo when and how to use wifi config API to change WiFI settings,
    Most WiFi APIs must be called in task scheduler, the system will work wrong if called in main(),
    For which API must be called in task, please refer to wifi_api.h or WiFi API reference.
    xTaskCreate(user_wifi_app_entry,
                UNIFY_USR_DEMO_TASK_NAME,
                UNIFY_USR_DEMO_TASK_STACKSIZE / 4,
                NULL, UNIFY_USR_DEMO_TASK_PRIO, NULL);
    user_wifi_app_entry is user's task entry function, which may be defined in another C file to do application job.
    UNIFY_USR_DEMO_TASK_NAME, UNIFY_USR_DEMO_TASK_STACKSIZE and UNIFY_USR_DEMO_TASK_PRIO should be defined
    in task_def.h. User needs to refer to example in task_def.h, then makes own task MACROs defined.
    */

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following line
    will never be reached.  If the following line does execute, then there was
    insufficient FreeRTOS heap memory available for the idle and/or timer tasks
    to be created.  See the memory management section on the FreeRTOS web site
    for more details. */
    for( ;; );
}

