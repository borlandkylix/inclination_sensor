#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

enum app_state{
	APP_STATE_INIT=0,
	APP_STATE_IDLE,
	APP_STATE_CONNECTED,
	APP_STATE_SENSING,
	APP_STATE_NUM,
};

enum incl_sensor_event_type{
	EVENT_NONE=0,
	EVENT_BLE,
	EVENT_SYSTEM,
    EVNET_SENSOR,
};

enum incl_sensor_ble_event_type{
	BLE_CONNECTED,
	BLE_DISCONNECTED,
	BLE_NOTIFY_ENABLED,
	BLE_NOTIFY_DISABLED,
};

enum inc_sensor_system_event_type{
	SYSTEM_INITIATED,
	SYSTEM_ERROR,
};

struct incl_sensor_event{
	enum incl_sensor_event_type type;
	union{
		enum incl_sensor_ble_event_type ble;
		enum inc_sensor_system_event_type system;
	};
};

#define IS_INCL_SENSOR_EVENT(event_ptr, ev_type, ev_subtype) \
    ((event_ptr)->type == (ev_type) && ( \
        ((ev_type) == EVENT_BLE) ? ((event_ptr)->ble == (enum incl_sensor_ble_event_type)(ev_subtype)) : \
        ((ev_type) == EVENT_SYSTEM) ? ((event_ptr)->system == (enum inc_sensor_system_event_type)(ev_subtype)) : false \
    ))

int start_ble(void);
int start_ble_adv(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */