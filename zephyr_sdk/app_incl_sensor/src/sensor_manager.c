#include <zephyr/kernel.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/can.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_manager, CONFIG_APP_LOG_LEVEL);

#include "common.h"

enum incl_sensor_index{
    INCL_SENSOR_LEFT = 0,
    INCL_SENSOR_RIGHT,
    INCL_SENSOR_NUM,
};

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
    INCL_SENSOR_EVENT_CAN_TIMEOUT,
};

enum incl_sensor_canbus_filter_type{
    CAN_FILTER_TPDO1 = 0,
    CAN_FILTER_EMERGENCY,
    CAN_FILTER_NMT_ERR_CTRL,
    CAN_FILTER_NUM,
};

enum incl_sensor_canbus_send_frame_type{
    INCL_SENSOR_CAN_FRAME_RTR = 0,
    INCL_SENSOR_CAN_FRAME_NMT_START_REMOTE_NODE,
    INCL_SENSOR_CAN_FRAME_SYNC,
    INCL_SENSOR_CAN_FRAME_NUM,
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

struct can_frame incl_sensor_canbus_frame[INCL_SENSOR_CAN_FRAME_NUM] = {
//Identifier (COB-ID): 0x700 + Node-ID
// Message Type: RTR (Remote Frame, DLC = 0), RTR bit set to 1
// Example: If the sensor's Node-ID is 5, send an RTR to 0x705.
    [INCL_SENSOR_CAN_FRAME_RTR] = {
        .id = 0x180, //TODO: confirm the ID
        .dlc = 0,
        .flags = CAN_FRAME_RTR,
    },
    [INCL_SENSOR_CAN_FRAME_NMT_START_REMOTE_NODE] = {
        .id = 0x000, //NMT frame has ID 0
        .dlc = 2,
        .data = {0x01, 0x00}, //Start remote node with ID 0 (broadcast)
    },
    [INCL_SENSOR_CAN_FRAME_SYNC] = {
        .id = 0x80, //TODO: confirm the ID
        .dlc = 0,
    },
};

const k_timeout_t incl_sensor_canbus_frame_period[INCL_SENSOR_CAN_FRAME_NUM] = {
    [INCL_SENSOR_CAN_FRAME_RTR] = K_MSEC(1000), //TODO: confirm the period
    [INCL_SENSOR_CAN_FRAME_NMT_START_REMOTE_NODE] = K_MSEC(1000), //NMT frame is sent once
    [INCL_SENSOR_CAN_FRAME_SYNC] = K_MSEC(1000), //TODO: confirm the period
};
struct incl_sensor_canbus_context{
    const struct device *dev;
    struct can_filter *filter;
    int filter_id[CAN_FILTER_NUM];
    struct k_msgq *	msgq;
};

struct incl_sensor_context{
    const struct device *reg_vsens;
	// const struct device *dev_left;
    // const struct device *dev_right;
    //TODO: use incl_sensor_canbus_context ?
    //struct k_sem sem;
    const struct device *const dev_can[INCL_SENSOR_NUM];
    struct k_msgq *const can_msgq[INCL_SENSOR_NUM];
    const struct can_filter *const can_ft;
    const struct can_frame *const can_frm;
    const k_timeout_t *const can_frm_period;
    int filter_id[INCL_SENSOR_NUM][CAN_FILTER_NUM];
    struct k_timer *can_timer;
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
static void can_timer_expiry_func(struct k_timer *timer_id); //forward declaration
CAN_MSGQ_DEFINE(can_msgq_left, 4);
CAN_MSGQ_DEFINE(can_msgq_right, 4);
K_TIMER_DEFINE(can_tmr, can_timer_expiry_func, NULL);
K_MSGQ_DEFINE(incl_sensor_sm_msgq, sizeof(enum inc_sensor_ctrl_event_type), 10, 1); 
static struct incl_sensor_context ctx = {
    .reg_vsens = DEVICE_DT_GET(DT_ALIAS(incl_sensor_reg)),
    // .dev_left = DEVICE_DT_GET(DT_ALIAS(can_left)),
    // .dev_right = DEVICE_DT_GET(DT_ALIAS(can_right)),
    //.sem = Z_SEM_INITIALIZER(ctx.sem, 0, 1),
    .dev_can = {DEVICE_DT_GET(DT_ALIAS(can_left)), DEVICE_DT_GET(DT_ALIAS(can_right))},
    .can_ft = incl_sensor_canbus_filter,
    .can_msgq = {&can_msgq_left, &can_msgq_right},
    .can_frm = incl_sensor_canbus_frame,
    .can_frm_period = incl_sensor_canbus_frame_period,
    .filter_id = {{-EINVAL, -EINVAL, -EINVAL}, {-EINVAL, -EINVAL, -EINVAL}},
    .can_timer = &can_tmr,
    .msgq = &incl_sensor_sm_msgq,
    .sm_states = incl_sensor_sm_states,
    .event = INCL_SENSOR_EVENT_NONE,
};

////////////////////////////////////////
/* operations needed by state machine */
////////////////////////////////////////
static void start_can(struct incl_sensor_context *ctx)
{
    for(int i = 0; i < INCL_SENSOR_NUM; i++){
        int err = can_set_mode(ctx->dev_can[i], CAN_MODE_NORMAL);
        if(err){
            LOG_ERR("can_set_mode() returns %d for sensor %d", err, i);
        }
        err = can_start(ctx->dev_can[i]);
        if(err){
            LOG_ERR("can_start() returns %d for sensor %d", err, i);
        }
    }
}

static void stop_can(struct incl_sensor_context *ctx)
{
    for(int i = 0; i < INCL_SENSOR_NUM; i++){
        can_stop(ctx->dev_can[i]);
    }
}

static void set_can_filter(struct incl_sensor_context *ctx)
{
    for(int i = 0; i < INCL_SENSOR_NUM; i++){
        for(int j = 0; j < CAN_FILTER_NUM; j++){
            if(ctx->filter_id[i][j] >= 0){
                continue; //filter already set
            }
            int err = can_add_rx_filter_msgq(ctx->dev_can[i], ctx->can_msgq[i], &incl_sensor_canbus_filter[j]);
            if(err < 0){
                LOG_ERR("can_add_rx_filter_msgq() returns %d for sensor %d filter %d", err, i, j);
            }
            else{
                ctx->filter_id[i][j] = err;
            }
        }
    }
}

static void remove_can_filter(struct incl_sensor_context *ctx)
{
    for(int i = 0; i < INCL_SENSOR_NUM; i++){
        for(int j = 0; j < CAN_FILTER_NUM; j++){
            if(ctx->filter_id[i][j] >= 0){
                can_remove_rx_filter(ctx->dev_can[i], ctx->filter_id[i][j]);
                ctx->filter_id[i][j] = -EINVAL;
            }
        }
    }
}

static void can_timer_start(struct incl_sensor_context *ctx, k_timeout_t duration, k_timeout_t period)
{
    k_timer_user_data_set(ctx->can_timer, ctx);
    k_timer_start(ctx->can_timer, duration, period);
}

static void can_timer_expiry_func(struct k_timer *timer_id)
{
    struct incl_sensor_context *ctx = (struct incl_sensor_context *)k_timer_user_data_get(timer_id);
    enum inc_sensor_ctrl_event_type event = INCL_SENSOR_EVENT_CAN_TIMEOUT;
    int err = k_msgq_put(ctx->msgq, &event, K_FOREVER);
	if(err){
		LOG_ERR("k_msgq_put() returns %d", err);
	}
}

static void send_can_frame(struct incl_sensor_context *ctx, enum incl_sensor_canbus_send_frame_type type)
{
    for(int i = 0; i < INCL_SENSOR_NUM; i++){
        int err = can_send(ctx->dev_can[i], &ctx->can_frm[type], K_NO_WAIT, NULL, NULL);
        if(err){
            LOG_ERR("can_send() returns %d for frame %d", err, i);
        }
    }

    can_timer_start(ctx, ctx->can_frm_period[type], K_NO_WAIT); //only trigger the timer once
}

/////////////////////////////
/* state machine functions */
/////////////////////////////
static void off_entry(void *o)
{
    LOG_DBG("off_entry()");
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    //turn off the regulator
    if(regulator_is_enabled(ctx->reg_vsens)){
        regulator_disable(ctx->reg_vsens);
    }

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
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    //turn on the regulator
    if(!regulator_is_enabled (ctx->reg_vsens)){
        regulator_enable(ctx->reg_vsens);
    }

    start_can(ctx);
    set_can_filter(ctx);
    send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_RTR);
}

static enum smf_state_result init_run(void *o)
{
	struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    //periodically sending RTR frame and monitor bootup or guarding message
    //if we receive expected message, transition to pre-operational state
    //we don't block on waiting CAN message here,
    //give CAN bus filter and trigger event in callback
    if(ctx->event == INCL_SENSOR_EVENT_CAN_TIMEOUT){
        //check the ready signal from CAN message queue, if not ready, keep sending RTR frame
        send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_RTR);
    }
    else{
        LOG_WRN("Unexpected event %d in init state", ctx->event);
    }
	return SMF_EVENT_HANDLED;
}

static void pre_operational_entry(void *o)
{
    LOG_DBG("pre_operational_entry()");
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_NMT_START_REMOTE_NODE);
    send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_RTR);
}

static enum smf_state_result pre_operational_run(void *o)
{
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    if(ctx->event == INCL_SENSOR_EVENT_CAN_TIMEOUT){
        //check the TPDO1 message from CAN message queue, if not received, keep sending NMT start frame
        send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_NMT_START_REMOTE_NODE);
        send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_RTR);
    }
    else{
        LOG_WRN("Unexpected event %d in pre-operational state", ctx->event);
    }
    return SMF_EVENT_HANDLED;
}

static void operational_entry(void *o)
{
    LOG_DBG("operational_entry()");
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_SYNC);
}
static enum smf_state_result operational_run(void *o)
{
    struct incl_sensor_context *ctx = CONTAINER_OF(o, struct incl_sensor_context, sm_ctx);
    if(ctx->event == INCL_SENSOR_EVENT_CAN_TIMEOUT){
        //check the TPDO1 message from CAN message queue, if not received, keep sending SYNC frame
        send_can_frame(ctx, INCL_SENSOR_CAN_FRAME_SYNC);
    }
    else{
        LOG_WRN("Unexpected event %d in operational state", ctx->event);
    }
    return SMF_EVENT_HANDLED;
}

static const struct smf_state incl_sensor_sm_states[] = {
    [INCL_SENSOR_OFF] = SMF_CREATE_STATE(off_entry, off_run, NULL, NULL, NULL),
    [INCL_SENSOR_INIT] = SMF_CREATE_STATE(init_entry, init_run, NULL, NULL, NULL),
    [INCL_SENSOR_PRE_OPERATIONAL] = SMF_CREATE_STATE(pre_operational_entry, pre_operational_run, NULL, NULL, NULL),
    [INCL_SENSOR_OPERATIONAL] = SMF_CREATE_STATE(operational_entry, operational_run, NULL, NULL, NULL),
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
    enum inc_sensor_ctrl_event_type event = INCL_SENSOR_CMD_START;
    int err = k_msgq_put(ctx.msgq, &event, K_FOREVER);
	if(err){
		LOG_ERR("k_msgq_put() returns %d", err);
	}
}

void incl_sensor_data_read_stop(void)
{
    enum inc_sensor_ctrl_event_type event = INCL_SENSOR_CMD_STOP;
    int err = k_msgq_put(ctx.msgq, &event, K_FOREVER);
	if(err){
		LOG_ERR("k_msgq_put() returns %d", err);
	}
}