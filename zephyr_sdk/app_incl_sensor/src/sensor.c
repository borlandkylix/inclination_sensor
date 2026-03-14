#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor, CONFIG_APP_LOG_LEVEL);

#include "sensor.h"


static void init_entry(void *o)
{
    LOG_DBG("init_entry()");

}

static enum smf_state_result init_run(void *o)
{
	struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    //periodically sending RTR frame and monitor bootup or guarding message
    //if we receive expected message, transition to pre-operational state
    //we don't block on waiting CAN message here,
    //give CAN bus filter and trigger event in callback
	return SMF_EVENT_HANDLED;
}

static const struct smf_state incl_sensor_sm_states[] = {
        [INCL_SENSOR_INIT] = SMF_CREATE_STATE(init_entry, init_run, NULL, NULL, NULL),
};

void incl_sensor_task(void *p1, void *p2, void *p3)
{
    //run a state machine, 
    struct incl_sensor_context *ctx = (struct incl_sensor_context*)p1;
    smf_set_initial(&ctx->sm_ctx, &ctx->sm_states[INCL_SENSOR_INIT]);

}

void incl_sensor_init(struct incl_sensor_context *ctx)
{
    //send trigger message to state machine
}

void incl_sensor_read(struct incl_sensor_context *ctx)
{

}

void incl_sensor_stop(struct incl_sensor_context *ctx)
{
    //send trigger message to state machine
}