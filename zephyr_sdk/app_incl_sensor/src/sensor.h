#ifndef SENSOR_H
#define SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/smf.h>

enum incl_sensor_state{
    INCL_SENSOR_INIT = 0,
    INCL_SENSOR_PRE_OPERATIONAL,
    INCL_SENSOR_OPERATIONAL,
    INCL_SENSOR_STOPPED,
    INCL_SENSOR_ERROR,
};

enum inc_sensor_ctrl_event_type{
	INCL_SENSOR_START,
	INCL_SENSOR_STOP,
};

struct incl_sensor_context{
    const struct device *dev;
    struct smf_ctx sm_ctx;
    const struct smf_state *sm_states;
    struct k_msgq* msgq;
    enum inc_sensor_ctrl_event_type event;
};
struct incl_sensor_data{
    int16_t x;
    int16_t y;
};

struct incl_sensor_cb{
    void (*sensor_operational)(struct incl_sensor_context* ctx);
    void (*data_ready)(const struct incl_sensor_data* data, struct incl_sensor_context* ctx);
    void (*error)(int error_code, struct incl_sensor_context* ctx);
    struct incl_sensor_context *ctx;
};

void incl_sensor_task(void *p1, void *p2, void *p3);

#define INCL_SENSOR_CONTEXT_DEFINE(name, can) \
    K_MSGQ_DEFINE(name##_msgq, sizeof(enum inc_sensor_ctrl_event_type), 10, 1); \
    const struct smf_state name##_sm_states[]; \
    K_THREAD_STACK_DEFINE(name##_thread_stack, CONFIG_INCL_SENSOR_THREAD_STACK_SIZE); \
     struct incl_sensor_context name##_ctx; \
    void name##_thread_fn(void *p1, void *p2, void *p3); \
    K_THREAD_DEFINE(name##_thread_id, CONFIG_INCL_SENSOR_THREAD_STACK_SIZE, name##_thread_fn, &name##_ctx, NULL, NULL, CONFIG_INCL_SENSOR_THREAD_PRIO, 0, 0);

void incl_sensor_init(struct incl_sensor_context *ctx);
void incl_sensor_read(struct incl_sensor_context *ctx);
void incl_sensor_stop(struct incl_sensor_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_H */