/* NimBLE GATT server for Niulai BLE GATT Protocol v1
 * (docs/BLE_PROTOCOL.md, Accepted).
 *
 * Service 7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11 with:
 *   CONTROL_STATE ...1001  encrypted bonded WRITE with response
 *   STATUS        ...1002  READ | NOTIFY (0x0012) + CCCD 0x2902
 *   HOME_COMMAND  ...1003  encrypted bonded WRITE with response
 *   IMU_TELEMETRY ...1004  READ | NOTIFY + CCCD 0x2902
 *   GAME_MODE     ...1005  encrypted bonded WRITE with response (display only)
 *   AUDIO_COMMAND ...1006  encrypted bonded WRITE with response
 * Advertises the complete 128-bit service UUID and device name "牛来快跑".
 *
 * The 128-bit UUID literals are written in on-air (little-endian) byte
 * order - byte-reversed from the canonical strings. The host test
 * (test_ble_protocol.c) reverses them back and compares against the
 * canonical strings; this byte order has broken in a previous team
 * implementation.
 */
#include "sdkconfig.h"

#include "ble_input.h"
#include "ble_protocol.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_store_config_init(void);

static const char *TAG = "ble_gatt";

static const ble_uuid128_t qw_svc_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_SVC_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_control_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_CONTROL_CHR_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_status_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_STATUS_CHR_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_home_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_HOME_CHR_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_telemetry_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_TELEMETRY_CHR_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_game_mode_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_GAME_MODE_CHR_UUID_LE_BYTES,
};
static const ble_uuid128_t qw_audio_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = QW_BLE_AUDIO_CHR_UUID_LE_BYTES,
};
static portMUX_TYPE s_link_lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_status_subscribed;
static bool s_telemetry_subscribed;
static uint16_t s_control_val_handle;
static uint16_t s_status_val_handle;
static uint16_t s_home_val_handle;
static uint16_t s_telemetry_val_handle;
static uint16_t s_game_mode_val_handle;
static uint16_t s_audio_val_handle;
static uint8_t s_own_addr_type;

static int qw_gap_event(struct ble_gap_event *event, void *arg);
static void qw_advertise(void);

/* Access callback. Kept short: validation + state update only. */
static int qw_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)arg;
    uint8_t buf[QW_BLE_TELEMETRY_PACKET_LEN];

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == s_status_val_handle) {
            ble_input_build_status_packet(buf);
            int rc = os_mbuf_append(ctxt->om, buf, QW_BLE_PACKET_LEN);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (attr_handle == s_telemetry_val_handle) {
            ble_input_build_telemetry_packet(buf);
            int rc = os_mbuf_append(ctxt->om, buf,
                                    QW_BLE_TELEMETRY_PACKET_LEN);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (!ble_input_link_secure()) {
            return BLE_ATT_ERR_INSUFFICIENT_ENC;
        }
        if (attr_handle == s_home_val_handle) {
            if (OS_MBUF_PKTLEN(ctxt->om) != QW_BLE_HOME_PACKET_LEN ||
                ble_hs_mbuf_to_flat(ctxt->om, buf,
                                   QW_BLE_HOME_PACKET_LEN, NULL) != 0 ||
                !qw_ble_validate_home_packet(buf, QW_BLE_HOME_PACKET_LEN) ||
                !ble_input_request_home()) {
                ble_input_log_invalid();
                return BLE_ATT_ERR_UNLIKELY;
            }
            return 0;
        }
        if (attr_handle == s_game_mode_val_handle) {
            if (OS_MBUF_PKTLEN(ctxt->om) != QW_BLE_GAME_MODE_PACKET_LEN ||
                ble_hs_mbuf_to_flat(ctxt->om, buf,
                                   QW_BLE_GAME_MODE_PACKET_LEN, NULL) != 0 ||
                buf[0] != QW_BLE_VERSION ||
                !ble_input_set_game_mode(buf[1])) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            return 0;
        }
        if (attr_handle == s_audio_val_handle) {
            if (OS_MBUF_PKTLEN(ctxt->om) != QW_BLE_AUDIO_PACKET_LEN ||
                ble_hs_mbuf_to_flat(ctxt->om, buf,
                                   QW_BLE_AUDIO_PACKET_LEN, NULL) != 0 ||
                !qw_ble_validate_audio_command(buf, QW_BLE_AUDIO_PACKET_LEN) ||
                !ble_input_request_mama_audio()) {
                ble_input_log_invalid();
                return BLE_ATT_ERR_UNLIKELY;
            }
            printf("BLE_AUDIO_RX command=play_mama\n");
            return 0;
        }
        if (attr_handle != s_control_val_handle) {
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
        if (OS_MBUF_PKTLEN(ctxt->om) != QW_BLE_PACKET_LEN) {
            ble_input_log_invalid();
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, QW_BLE_PACKET_LEN, NULL) != 0) {
            ble_input_log_invalid();
            return BLE_ATT_ERR_UNLIKELY;
        }
        {
            uint8_t seq, mask;
            if (!qw_ble_validate_control_packet(buf, QW_BLE_PACKET_LEN, &seq,
                                                 &mask)) {
                ble_input_log_invalid();
                return BLE_ATT_ERR_UNLIKELY;
            }
            /* A HOME transition is a real admission gate, not a silently
             * successful write. Keep Write With Response meaningful. */
            if (!ble_input_accept_packet(seq, mask)) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            return 0;
        }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def qw_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &qw_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                /* CONTROL_STATE: encrypted bonded write with response */
                .uuid = &qw_control_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &s_control_val_handle,
            },
            {
                /* NimBLE automatically creates the CCCD for NOTIFY. */
                .uuid = &qw_status_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_val_handle,
            },
            {
                /* HOME_COMMAND: encrypted bonded write, consumed by control task */
                .uuid = &qw_home_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &s_home_val_handle,
            },
            {
                /* NimBLE automatically creates the CCCD for NOTIFY. */
                .uuid = &qw_telemetry_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_telemetry_val_handle,
            },
            {
                /* GAME_MODE: display-only encrypted bonded write. */
                .uuid = &qw_game_mode_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &s_game_mode_val_handle,
            },
            {
                /* AUDIO_COMMAND: encrypted bonded write; no motion side effect. */
                .uuid = &qw_audio_chr_uuid.u,
                .access_cb = qw_gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &s_audio_val_handle,
            },
            {0},
        },
    },
    {0},
};

static void qw_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                void *arg)
{
    (void)arg;
    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        printf("BLE_GATT_REGISTER service=7d4e1000-8a52-4d9e-b0f8-57e6c59e2e11\n");
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        if (ctxt->chr.chr_def->uuid == &qw_control_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1001-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=WRITE|WRITE_ENC\n");
        } else if (ctxt->chr.chr_def->uuid == &qw_status_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1002-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=0x0012\n");
        } else if (ctxt->chr.chr_def->uuid == &qw_home_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1003-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=WRITE|WRITE_ENC\n");
        } else if (ctxt->chr.chr_def->uuid == &qw_telemetry_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1004-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=0x0012\n");
        } else if (ctxt->chr.chr_def->uuid == &qw_game_mode_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1005-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=WRITE|WRITE_ENC display_only=1\n");
        } else if (ctxt->chr.chr_def->uuid == &qw_audio_chr_uuid.u) {
            printf("BLE_GATT_REGISTER chr=7d4e1006-8a52-4d9e-b0f8-57e6c59e2e11 "
                   "props=WRITE|WRITE_ENC command=PLAY_MAMA\n");
        }
        break;
    default:
        break;
    }
}

void ble_gatt_notify_status(const uint8_t pkt[QW_BLE_PACKET_LEN])
{
    struct os_mbuf *om;
    uint16_t conn_handle;

    portENTER_CRITICAL(&s_link_lock);
    conn_handle = s_conn_handle;
    bool subscribed = s_status_subscribed;
    portEXIT_CRITICAL(&s_link_lock);

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !subscribed) {
        return;
    }
    om = ble_hs_mbuf_from_flat(pkt, QW_BLE_PACKET_LEN);
    if (om == NULL) {
        return;
    }
    /* Safe from other tasks: NimBLE host calls take the host lock. */
    ble_gatts_notify_custom(conn_handle, s_status_val_handle, om);
}

void ble_gatt_notify_telemetry(
    const uint8_t pkt[QW_BLE_TELEMETRY_PACKET_LEN])
{
    struct os_mbuf *om;
    uint16_t conn_handle;
    portENTER_CRITICAL(&s_link_lock);
    conn_handle = s_conn_handle;
    bool subscribed = s_telemetry_subscribed;
    portEXIT_CRITICAL(&s_link_lock);
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !subscribed) return;
    om = ble_hs_mbuf_from_flat(pkt, QW_BLE_TELEMETRY_PACKET_LEN);
    if (om == NULL) return;
    ble_gatts_notify_custom(conn_handle, s_telemetry_val_handle, om);
}

static void qw_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"牛来快跑";
    fields.name_len = strlen("牛来快跑");
    fields.name_is_complete = 1;
    /* Complete 128-bit service UUID (Android filters on it). */
    fields.uuids128 = &qw_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           qw_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
    }
}

static int qw_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "GAP connect status=%d conn=%u",
                 event->connect.status, event->connect.conn_handle);
        ble_input_set_link_secure(false);
        portENTER_CRITICAL(&s_link_lock);
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_status_subscribed = false;
            s_telemetry_subscribed = false;
        } else {
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_status_subscribed = false;
            s_telemetry_subscribed = false;
        }
        portEXIT_CRITICAL(&s_link_lock);
        if (event->connect.status != 0) {
            qw_advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "GAP disconnect conn=%u reason=%d",
                 event->disconnect.conn.conn_handle,
                 event->disconnect.reason);
        portENTER_CRITICAL(&s_link_lock);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_status_subscribed = false;
        s_telemetry_subscribed = false;
        portEXIT_CRITICAL(&s_link_lock);
        ble_input_handle_disconnect();
        qw_advertise(); /* resume advertising for the next manual reconnect */
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc;
        bool secure_link = false;
        if (event->enc_change.status == 0 &&
            ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
            secure_link = desc.sec_state.encrypted && desc.sec_state.bonded;
        }
        ble_input_set_link_secure(secure_link);
        ESP_LOGI(TAG, "BLE security conn=%u encrypted_bonded=%d",
                 event->enc_change.conn_handle, secure_link ? 1 : 0);
        return 0;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        /* No-PIN pairing uses LE Secure Connections Just Works. Any passkey
         * request means that the peer is asking for a security mode this
         * endpoint deliberately does not provide. */
        ESP_LOGE(TAG, "BLE_PAIRING_PIN_UNSUPPORTED action=%d",
                 event->passkey.params.action);
        return BLE_HS_EAPP;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) != 0) {
            return BLE_HS_ECONTROLLER;
        }
        (void)ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_status_val_handle) {
            portENTER_CRITICAL(&s_link_lock);
            s_status_subscribed = event->subscribe.cur_notify != 0;
            bool subscribed = s_status_subscribed;
            portEXIT_CRITICAL(&s_link_lock);
            if (subscribed) {
                ble_input_notify_status();
            }
        } else if (event->subscribe.attr_handle == s_telemetry_val_handle) {
            portENTER_CRITICAL(&s_link_lock);
            s_telemetry_subscribed = event->subscribe.cur_notify != 0;
            portEXIT_CRITICAL(&s_link_lock);
            if (event->subscribe.cur_notify != 0) {
                ble_input_notify_telemetry();
            }
        }
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        portENTER_CRITICAL(&s_link_lock);
        bool disconnected = s_conn_handle == BLE_HS_CONN_HANDLE_NONE;
        portEXIT_CRITICAL(&s_link_lock);
        if (disconnected) {
            qw_advertise();
        }
        return 0;
    default:
        return 0;
    }
}

static void qw_on_reset(int reason)
{
    ble_input_handle_disconnect();
    portENTER_CRITICAL(&s_link_lock);
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_status_subscribed = false;
    s_telemetry_subscribed = false;
    portEXIT_CRITICAL(&s_link_lock);
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void qw_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }
    qw_advertise();
}

static void qw_host_task(void *param)
{
    (void)param;
    nimble_port_run(); /* returns only when nimble_port_stop() is executed */
    nimble_port_freertos_deinit();
}

void ble_gatt_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* The default NVS partition is shared with application data such as
         * voice templates. Never erase it implicitly during BLE startup. */
        ESP_LOGE(TAG, "NVS needs maintenance; refusing global erase: %s",
                 esp_err_to_name(ret));
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return;
    }
    /* ble_gatts_notify_custom() can be called by the control task.  NimBLE's
     * per-notification INFO trace is synchronous UART output on that caller's
     * stack and can starve the control task.  Keep errors/warnings visible. */
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    ble_hs_cfg.reset_cb = qw_on_reset;
    ble_hs_cfg.sync_cb = qw_on_sync;
    ble_hs_cfg.gatts_register_cb = qw_gatt_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    /* Physical actuation requires an encrypted bonded link. This bring-up
     * profile intentionally uses LE Secure Connections without MITM so the
     * phone can pair without a PIN; the runtime safety gate remains required. */
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_sc_only = 0;
    ble_hs_cfg.sm_sec_lvl = 2;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    if (ble_gatts_count_cfg(qw_gatt_svcs) != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed");
        return;
    }
    if (ble_gatts_add_svcs(qw_gatt_svcs) != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed");
        return;
    }

    /* The service table evolved after the first bonded Android clients were
     * paired (HOME_COMMAND and IMU_TELEMETRY were added). Mark the complete
     * attribute range changed so NimBLE can send the standard Service Changed
     * indication to cached bonded centrals on their next connection. */
    ble_svc_gatt_changed(0x0001, 0xffff);

    ble_svc_gap_device_name_set("牛来快跑");
    ble_store_config_init();

    nimble_port_freertos_init(qw_host_task);
}
