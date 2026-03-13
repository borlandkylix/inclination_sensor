#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_manager, CONFIG_APP_LOG_LEVEL);

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/zbus/zbus.h>

#include "common.h"
#include "incl_svc.h"

ZBUS_CHAN_DECLARE(incl_sensor_event_chan);

static struct bt_conn *default_conn = NULL;

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (default_conn == NULL) {
		default_conn = bt_conn_ref(conn);
	}

	if (conn != default_conn) {
		return;
	}

    LOG_INF("BT Connected: %s\n", addr);

    struct incl_sensor_event event = {
        .type = EVENT_BLE,
        .ble = BLE_CONNECTED,
    };
	err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
	if (err < 0){
		LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
	}
	
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	bt_conn_unref(default_conn);
	default_conn = NULL;

    struct incl_sensor_event event = {
        .type = EVENT_BLE,
        .ble = BLE_DISCONNECTED,
    };
	int err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
	if (err < 0){
		LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	LOG_INF("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
}

static struct bt_gatt_cb gatt_callbacks = {.att_mtu_updated = mtu_updated};

static void ble_auth_cancel_cb(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("BLE Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb ble_auth_cb_display = {
	.cancel = ble_auth_cancel_cb
};


void incl_svc_notify_enabled_cb(bool enabled, void *ctx)
{
    LOG_INF("incl_svc_notify_enabled_cb: enabled=%d\n", enabled);
    if(enabled){
        struct incl_sensor_event event = {
            .type = EVENT_BLE,
            .ble = BLE_NOTIFY_ENABLED,
        };
        int err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
        if (err < 0){
            LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
        }
    }
    else{
        struct incl_sensor_event event = {
            .type = EVENT_BLE,
            .ble = BLE_NOTIFY_DISABLED,
        };
        int err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
        if (err < 0){
            LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
        }
    }
}

struct bt_incl_svc_cb incl_svc_callbacks = {
    .notify_enabled = incl_svc_notify_enabled_cb, 
    .ctx = NULL,
};

int start_ble(void)
{
    LOG_INF("Startup Bluetooth\n");
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable() returns error %d", err);
        return err;
	}
    bt_conn_cb_register(&conn_callbacks);
	bt_gatt_cb_register(&gatt_callbacks);
    incl_svc_cb_register(&incl_svc_callbacks);

    return 0;
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_INCL_SVC_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void adv_restart_fn(struct k_work *work)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
		LOG_ERR("BLE Advertising failed to start (error %d)\n", err);
        return;
	} else {
		LOG_INF("BLE Advertising started");
#if defined(CONFIG_BT_SMP)
        bt_conn_auth_cb_register(&ble_auth_cb_display);
#endif
    }
    // struct incl_sensor_event event = {
    //     .type = EVENT_BLE,
    //     .ble = BLE_ADV_STARTED,
    // };
	// err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
	// if (err < 0){
	// 	LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
	// }	
}

K_WORK_DELAYABLE_DEFINE(adv_restart_work, adv_restart_fn);

int start_ble_adv(void)
{
    LOG_INF("Start Bluetooth Advertising\n");
    //intentional delay to avoid immediately starting adv
    //right after a connection is disconnected
    int err =k_work_schedule(&adv_restart_work, K_MSEC(100));
    if(err < 0){
        LOG_ERR("k_work_schedule() returns error %d", err);
        return err;
    }
    return 0;
}

static void incl_sensor_data_read_cb(const struct zbus_channel *chan)
{
	const struct incl_sensor_data *data = zbus_chan_const_msg(chan);
    //send BLE notification
    incl_svc_send_data(data);
}
ZBUS_LISTENER_DEFINE(incl_sensor_data_read_handler, incl_sensor_data_read_cb);