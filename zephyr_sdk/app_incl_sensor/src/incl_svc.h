#ifndef INCL_SVC_H
#define INCL_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief UUID of the NUS Service. **/
#define BT_UUID_INCL_SVC_VAL \
	BT_UUID_128_ENCODE(0x073c0000, 0xfd08, 0x42d9, 0xbdc9, 0x7f63a3a3ee5c)

/** @brief UUID of the TX Characteristic. **/
#define BT_UUID_INCL_SVC_RPT_VAL \
	BT_UUID_128_ENCODE(0x073c0001, 0xfd08, 0x42d9, 0xbdc9, 0x7f63a3a3ee5c)


#define BT_UUID_INCL_SVC   BT_UUID_DECLARE_128(BT_UUID_INCL_SVC_VAL)
#define BT_UUID_INCL_SVC_RPT        BT_UUID_DECLARE_128(BT_UUID_INCL_SVC_RPT_VAL)

/** @brief GHSS callbacks */
struct bt_incl_svc_cb{
	/** @brief Notifications subscription changed
	 *
	 * @param enabled Flag that is true if notifications were enabled, false
	 *                if they were disabled.
	 * @param ctx User context provided in the callback structure.
	 */
	void (*notify_enabled)(bool enabled, void *ctx);

	/** @brief Received Data
	 *
	 * @param conn Peer Connection object.
	 * @param data Pointer to buffer with data received.
	 * @param len Size in bytes of data received.
	 * @param ctx User context provided in the callback structure.
	 */
	//void (*received)(struct bt_conn *conn, const void *data, uint16_t len, void *ctx);

	/** Internal member. Provided as a callback argument for user context */
	void *ctx;

    /** Internal member to form a list of callbacks */
	sys_snode_t _node;
};

int incl_svc_cb_register(struct bt_incl_svc_cb *cb);
int incl_svc_send_data(const struct incl_sensor_system_data *data);

#ifdef __cplusplus
}
#endif

#endif /* INCL_SVC_H */