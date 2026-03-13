/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>

#include <app_version.h>

#include "common.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

ZBUS_CHAN_DEFINE(incl_sensor_event_chan,  /* Name */
		 struct incl_sensor_event, /* Message type */
		 NULL, /* Validator */
		 NULL, /* User data */
		 ZBUS_OBSERVERS(incl_sensor_event_handler), /* observers */
		 ZBUS_MSG_INIT(.type = EVENT_NONE)		      /* Initial value {0} */
);

//forward declaration
static const struct smf_state incl_sensor_sm_states[];

struct incl_sensor_state_machine{
	struct smf_ctx sm_ctx;
	const struct smf_state *sm_states;
	struct k_msgq* msgq;
	struct incl_sensor_event event;
};

/* USB state machine */
K_MSGQ_DEFINE(sm_msgq, sizeof(struct incl_sensor_event), 10, 1); 
static struct incl_sensor_state_machine incl_sensor_sm = {.msgq = &sm_msgq, .sm_states = incl_sensor_sm_states};

static void init_entry(void *o)
{
	LOG_DBG("init_entry()");
	int err = start_ble();
	if (err) {
		LOG_ERR("Failed to start BLE");
		//TODO: post error event to state machine
	}

	//TODO: Other initialization such as sensors, adc, etc

    struct incl_sensor_event event = {
        .type = EVENT_SYSTEM,
        .system = SYSTEM_INITIATED,
    };
	err = zbus_chan_pub(&incl_sensor_event_chan, &event, K_FOREVER);
	if (err < 0){
		LOG_ERR("zbus_chan_pub(&incl_sensor_event_chan) error (%d)\n", err);
	}	
}

static enum smf_state_result init_run(void *o)
{
	struct incl_sensor_state_machine *sm = CONTAINER_OF(o, struct incl_sensor_state_machine, sm_ctx);
	if(IS_INCL_SENSOR_EVENT(&sm->event, EVENT_SYSTEM, SYSTEM_INITIATED)){
		smf_set_state(&sm->sm_ctx, &incl_sensor_sm_states[APP_STATE_IDLE]);
	}
	else{
		LOG_WRN("Unexpected event %d,%d in init state", sm->event.type, sm->event.ble);
	}
	return SMF_EVENT_HANDLED;
}

static void idle_entry(void *o)
{
	LOG_DBG("init_entry()");
	int err = start_ble_adv();
	if (err) {
		LOG_ERR("Failed to start BLE");
		//TODO: post error event to state machine
	}
}

static enum smf_state_result idle_run(void *o)
{
	struct incl_sensor_state_machine *sm = CONTAINER_OF(o, struct incl_sensor_state_machine, sm_ctx);
	if(IS_INCL_SENSOR_EVENT(&sm->event, EVENT_BLE, BLE_CONNECTED)){
		smf_set_state(&sm->sm_ctx, &incl_sensor_sm_states[APP_STATE_CONNECTED]);
	}
	else{
		LOG_WRN("Unexpected event %d,%d in idle state", sm->event.type, sm->event.ble);
	}
	return SMF_EVENT_HANDLED;
}

static void connected_entry(void *o)
{
	LOG_DBG("connected_entry()");
}

static enum smf_state_result connected_run(void *o)
{
	struct incl_sensor_state_machine *sm = CONTAINER_OF(o, struct incl_sensor_state_machine, sm_ctx);
	if(IS_INCL_SENSOR_EVENT(&sm->event, EVENT_BLE, BLE_DISCONNECTED)){
		smf_set_state(&sm->sm_ctx, &incl_sensor_sm_states[APP_STATE_IDLE]);
	}
	else if(IS_INCL_SENSOR_EVENT(&sm->event, EVENT_BLE, BLE_NOTIFY_ENABLED)){
		smf_set_state(&sm->sm_ctx, &incl_sensor_sm_states[APP_STATE_SENSING]);
	}
	else{
		LOG_WRN("Unexpected event %d,%d in connected state", sm->event.type, sm->event.ble);
	}
	return SMF_EVENT_HANDLED;
}

static void sensing_entry(void *o)
{
	LOG_DBG("sensing_entry()");
	incl_sensor_data_read_start();
}

static enum smf_state_result sensing_run(void *o)
{
	struct incl_sensor_state_machine *sm = CONTAINER_OF(o, struct incl_sensor_state_machine, sm_ctx);
	if(IS_INCL_SENSOR_EVENT(&sm->event, EVENT_BLE, BLE_NOTIFY_DISABLED)){
		smf_set_state(&sm->sm_ctx, &incl_sensor_sm_states[APP_STATE_CONNECTED]);
	}
	else{
		LOG_WRN("Unexpected event %d,%d in sensing state", sm->event.type, sm->event.ble);
	}
	return SMF_EVENT_HANDLED;
}

static void sensing_exit(void *o)
{
	LOG_DBG("sensing_exit()");
	incl_sensor_data_read_stop();
}

static const struct smf_state incl_sensor_sm_states[] = {
        [APP_STATE_INIT] = SMF_CREATE_STATE(init_entry, init_run, NULL, NULL, NULL),
		[APP_STATE_IDLE] = SMF_CREATE_STATE(idle_entry, idle_run, NULL, NULL, NULL),
        [APP_STATE_CONNECTED] = SMF_CREATE_STATE(connected_entry, connected_run, NULL, NULL, NULL),
        [APP_STATE_SENSING] = SMF_CREATE_STATE(sensing_entry, sensing_run, sensing_exit, NULL, NULL),
};

static void incl_sensor_event_cb(const struct zbus_channel *chan)
{
	const struct incl_sensor_event *event = zbus_chan_const_msg(chan);
	int err = k_msgq_put(incl_sensor_sm.msgq, event, K_FOREVER);
	if(err){
		LOG_ERR("k_msgq_put() returns %d", err);
	}
}
ZBUS_LISTENER_DEFINE(incl_sensor_event_handler, incl_sensor_event_cb);

int main(void)
{
	int err;
	LOG_INF("PurplePedal App Version: v%u.%u.%u", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);

	smf_set_initial(&incl_sensor_sm.sm_ctx, &incl_sensor_sm_states[APP_STATE_INIT]);
	while(1){
		//k_msleep(1000);
		err = k_msgq_get(incl_sensor_sm.msgq, &incl_sensor_sm.event, K_FOREVER);
		if(err == 0){
			err = smf_run_state(&incl_sensor_sm.sm_ctx);
			if(err){
				LOG_INF("%s terminating state machine thread", __func__);
				break;
			}
		}
		else{
			LOG_ERR("k_msgq_get() returns %d", err);
		}
	}
	return 0;
}