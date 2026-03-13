#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(incl_svc, CONFIG_APP_LOG_LEVEL);

#include "common.h"
#include "incl_svc.h"

//single linked list of callbacks, if there is a need for context,
//this can be put into a big context struct
static sys_slist_t incl_svc_cbs = SYS_SLIST_STATIC_INIT(&incl_svc_cbs);

static void incl_svc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    //see example implementation in 
    //https://github.com/nrfconnect/sdk-nrf/blob/main/subsys/bluetooth/services/nus.c#L18
    //https://github.com/zephyrproject-rtos/zephyr/blob/69a8ac6a020d19d76c6fb6155072b5573f5e0013/subsys/bluetooth/services/nus/nus.c
	struct bt_incl_svc_cb *listener;
	SYS_SLIST_FOR_EACH_CONTAINER(&incl_svc_cbs, listener, _node) {
		if (listener->notify_enabled) {
			listener->notify_enabled((value == 1), listener->ctx);
		}
	}	
}

BT_GATT_SERVICE_DEFINE(incl_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_INCL_SVC),
	BT_GATT_CHARACTERISTIC(BT_UUID_INCL_SVC_RPT,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       NULL, NULL, NULL),
	BT_GATT_CCC(incl_svc_ccc_cfg_changed,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

);

#define GATT_CHARACTERISTIC_INDEX_INCL_SENSOR_RPT (1)

int incl_svc_cb_register(struct bt_incl_svc_cb *cb)
{
	if(!cb || !cb->notify_enabled){
		return -EINVAL;
	}
	sys_slist_append(&incl_svc_cbs, &cb->_node);
	return 0;
}

int incl_svc_send_data(const struct incl_sensor_data *data)
{	
	if(!data){
		return -EINVAL;
	}
	int err = bt_gatt_notify(NULL, &incl_svc.attrs[GATT_CHARACTERISTIC_INDEX_INCL_SENSOR_RPT], data, sizeof(struct incl_sensor_data));
	if(err){
		LOG_ERR("bt_gatt_notify() returns %d", err);
	}
	return err;
}