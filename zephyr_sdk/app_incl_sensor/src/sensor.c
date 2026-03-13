#include <canopennode.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor, CONFIG_APP_LOG_LEVEL);

#include "common.h"

struct incl_sensor_read_context{
	const struct device *dev_left;
    const struct device *dev_right;
    struct k_sem sem;
} ;

ZBUS_CHAN_DEFINE(incl_sensor_data_read_chan,  /* Name */
		 struct incl_sensor_data, /* Message type */
		 NULL, /* Validator */
		 NULL, /* User data */
		 ZBUS_OBSERVERS(incl_sensor_data_read_handler), /* observers */
		 ZBUS_MSG_INIT(.left_x = 0, .left_y = 0, .right_x = 0, .right_y = 0, .error_code = 0) /* Initial value {0} */
);

struct incl_sensor_read_context ctx = {
    .dev_left = NULL,
    .dev_right = NULL,
    .sem = Z_SEM_INITIALIZER(ctx.sem, 0, 1),
};

static void incl_sensor_read_task(void *p1, void *p2, void *p3)
{
    //TODO: Initialize CANopenNode and sensors
    while(1){
        k_sem_take(&ctx.sem, K_FOREVER);
        //TODO: read sensor
        struct incl_sensor_data data = {
            .left_x = 0,
            .left_y = 0,
            .right_x = 0,
            .right_y = 0,
            .error_code = 0,
        };
        int err = zbus_chan_pub(&incl_sensor_data_read_chan, &data, K_FOREVER);
        if(err){
            LOG_ERR("zbus_chan_pub() returns %d", err); 
        }

    }

}

K_THREAD_DEFINE(incl_sensor_read_thread_id, 
            CONFIG_INCL_SENSOR_READ_THREAD_STACK_SIZE, 
            incl_sensor_read_task, 
            &ctx, NULL, NULL, 
            CONFIG_INCL_SENSOR_READ_THREAD_PRIO, 
            0, 0);

static void incl_sensor_data_read_timer_handler(struct k_timer *timer)
{
	struct incl_sensor_read_context *ctx = (struct incl_sensor_read_context *)k_timer_user_data_get(timer);
	k_sem_give(&ctx->sem);
}
static K_TIMER_DEFINE(incl_sensor_data_read_timer, incl_sensor_data_read_timer_handler, NULL);

void incl_sensor_data_read_start(void)
{
    k_timer_user_data_set(&incl_sensor_data_read_timer, &ctx);
    k_timer_start(&incl_sensor_data_read_timer, K_MSEC(5000), K_MSEC(5000));
}

void incl_sensor_data_read_stop(void)
{
    k_timer_stop(&incl_sensor_data_read_timer);
}