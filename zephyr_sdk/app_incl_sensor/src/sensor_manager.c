#include <zephyr/drivers/can.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_manager, CONFIG_APP_LOG_LEVEL);

#include "common.h"

enum incl_sensor_state{
    INCL_SENSOR_OFF = 0,
    INCL_SENSOR_INIT,
    INCL_SENSOR_PRE_OPERATIONAL,
    INCL_SENSOR_OPERATIONAL,
    INCL_SENSOR_STOPPED,
    INCL_SENSOR_ERROR, //TODO: this one maybe not needed
};

enum inc_sensor_ctrl_event_type{
    INCL_SENSOR_EVENT_NONE = 0,
	INCL_SENSOR_CMD_START,
	INCL_SENSOR_CMD_STOP,
};

enum incl_sensor_canbus_filter_type{
    CAN_FILTER_TPDO1 = 0,
    CAN_FILTER_EMERGENCY,
    CAN_FILTER_NMT_ERR_CTRL,
    CAN_FILTER_NUM,
};

const struct can_filter incl_sensor_canbus_filter[CAN_FILTER_NUM] = {
    [CAN_FILTER_TPDO1] = {
        .id = 0x180, //TODO: confirm the ID
        .mask = CAN_STD_ID_MASK,
        .flags = 0U,
    },
    [CAN_FILTER_EMERGENCY] = {
        .id = 0x80, //TODO: confirm the ID
        .mask = CAN_STD_ID_MASK,
        .flags = 0U,
    },
    [CAN_FILTER_NMT_ERR_CTRL] = {
        .id = 0x700, //TODO: confirm the ID
        .mask = CAN_STD_ID_MASK,
        .flags = 0U,
    },
};

struct incl_sensor_canbus_context{
    const struct device *dev;
    struct can_filter *filter;
    int filter_id[CAN_FILTER_NUM];
    struct k_msgq *	msgq;
};

struct incl_sensor_context{
    const struct device *reg_vsens;
	const struct device *dev_left;
    const struct device *dev_right;
    //TODO: use incl_sensor_canbus_context
    //struct k_sem sem;
    struct smf_ctx sm_ctx;
    const struct smf_state *sm_states;
    struct k_msgq* msgq;
    enum inc_sensor_ctrl_event_type event;
} ;

ZBUS_CHAN_DEFINE(incl_sensor_data_read_chan,  /* Name */
		 struct incl_sensor_system_data, /* Message type */
		 NULL, /* Validator */
		 NULL, /* User data */
		 ZBUS_OBSERVERS(incl_sensor_data_read_handler), /* observers */
		 ZBUS_MSG_INIT(.left_x = 0, .left_y = 0, .right_x = 0, .right_y = 0, .error_code = 0) /* Initial value {0} */
);

static const struct smf_state incl_sensor_sm_states[]; //forward declaration
K_MSGQ_DEFINE(incl_sensor_sm_msgq, sizeof(enum inc_sensor_ctrl_event_type), 10, 1); 
static struct incl_sensor_context ctx = {
    .reg_vsens = DEVICE_DT_GET(DT_ALIAS(incl_sensor_reg)),
    .dev_left = DEVICE_DT_GET(DT_ALIAS(can_left)),
    .dev_right = DEVICE_DT_GET(DT_ALIAS(can_right)),
    //.sem = Z_SEM_INITIALIZER(ctx.sem, 0, 1),
    .msgq = &incl_sensor_sm_msgq,
    .sm_states = incl_sensor_sm_states,
    .event = INCL_SENSOR_EVENT_NONE,
};

static void off_entry(void *o)
{
    LOG_DBG("off_entry()");
    //turn on the regulator
    //set up CAn bus filter and callback
}

static enum smf_state_result off_run(void *o)
{
	struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    if(ctx->event == INCL_SENSOR_CMD_START){
        smf_set_state(&ctx->sm_ctx, &incl_sensor_sm_states[INCL_SENSOR_INIT]);
    }
    else{
        LOG_WRN("Unexpected event %d in off state", ctx->event);
    }
	return SMF_EVENT_HANDLED;
}

static void init_entry(void *o)
{
    LOG_DBG("init_entry()");
    //turn on the regulator
    //set up CAn bus filter and callback
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

// static void incl_sensor_read_task_old(void *p1, void *p2, void *p3)
// {
//     //TODO: Initialize CANopenNode and sensors
//     while(1){
//         k_sem_take(&ctx.sem, K_FOREVER);
//         //TODO: read sensor
//         struct incl_sensor_system_data data = {
//             .left_x = 0,
//             .left_y = 0,
//             .right_x = 0,
//             .right_y = 0,
//             .error_code = 0,
//         };
//         int err = zbus_chan_pub(&incl_sensor_data_read_chan, &data, K_FOREVER);
//         if(err){
//             LOG_ERR("zbus_chan_pub() returns %d", err); 
//         }

//     }

// }

static void incl_sensor_sm_task(void *p1, void *p2, void *p3)
{
    struct incl_sensor_context *ctx = (struct incl_sensor_context*)p1;
    smf_set_initial(&ctx->sm_ctx, &ctx->sm_states[INCL_SENSOR_OFF]);
    while(1){
        enum inc_sensor_ctrl_event_type event;
        int err = k_msgq_get(ctx->msgq, &event, K_FOREVER);
        if(err){
            LOG_ERR("k_msgq_get() returns %d", err); 
            continue;
        }
        ctx->event = event;
        smf_run_state(&ctx->sm_ctx);
    }
}

K_THREAD_DEFINE(incl_sensor_read_thread_id, 
            CONFIG_INCL_SENSOR_READ_THREAD_STACK_SIZE, 
            incl_sensor_sm_task, 
            &ctx, NULL, NULL, 
            CONFIG_INCL_SENSOR_READ_THREAD_PRIO, 
            0, 0);

// static void incl_sensor_data_read_timer_handler(struct k_timer *timer)
// {
// 	struct incl_sensor_context *ctx = (struct incl_sensor_context *)k_timer_user_data_get(timer);
// 	k_sem_give(&ctx->sem);
// }
// static K_TIMER_DEFINE(incl_sensor_data_read_timer, incl_sensor_data_read_timer_handler, NULL);

void incl_sensor_data_read_start(void)
{
    // k_timer_user_data_set(&incl_sensor_data_read_timer, &ctx);
    // k_timer_start(&incl_sensor_data_read_timer, K_MSEC(5000), K_MSEC(5000));
}

void incl_sensor_data_read_stop(void)
{
    // k_timer_stop(&incl_sensor_data_read_timer);
}