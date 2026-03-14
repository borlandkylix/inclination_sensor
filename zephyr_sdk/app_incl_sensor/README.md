# 1. Inclination Sensor Application

## 1.0 Initialize the environment:
```sh
west init -l zephyr_sdk
west update
west blobs fetch hal_espressif
sudo /opt/python/venv/bin/west packages pip --install
```


## 1.1 Build the application

After initializing workspace with west init and west update, call below command to apply the zephyr patches.

We need some ad7124 driver modification to include the features needed by InclinationSensor.

```shell
west patch apply
```

To build the application:

```shell
west build -b esp32c6_devkitc/esp32c6/hpcore -S flash-8M zephyr/samples/bluetooth/peripheral_hr -- -DESPRESSIF_TOOLCHAIN_PATH=$ESPRESSIF_TOOLCHAIN_PATH
=


west build -b esp32c6_devkitc/esp32c6/hpcore -S flash-8M zephyr/samples/bluetooth/peripheral_hr -- -DCONFIG_DEBUG_THREAD_INFO=y -DOPENOCD=$ESPRESSIF_TOOLCHAIN_PATH/openocd-esp32/bin/openocd -DOPENOCD_DEFAULT_PATH=$ESPRESSIF_TOOLCHAIN_PATH/openocd-esp32/share/openocd/scripts

west build -b esp32c6_devkitc/esp32c6/hpcore -S flash-8M zephyr_sdk/app_incl_sensor/ -- -DOPENOCD=$ESPRESSIF_TOOLCHAIN_PATH/openocd-esp32/bin/openocd -DOPENOCD_DEFAULT_PATH=$ESPRESSIF_TOOLCHAIN_PATH/openocd-esp32/share/openocd/scripts

west espressif monitor -p /dev/ttyUSB0
```

## 1.2 Debug ESP32C6

To debug ESP32C6, udev rules shall be properly configured on WSL (Not in Docker!):

Download the 60-openocd.rules, note that the version shall match the Dockerfile openocd-esp32 version:

https://github.com/espressif/openocd-esp32/blob/v0.12.0-esp32-20260304/contrib/60-openocd.rules

```
/opt/espressif/openocd-esp32/bin/openocd -s /workdir/zephyr/boards/espressif/esp32c6_devkitc/support -s /opt/espressif/openocd-esp32/share/openocd/scripts -f /workdir/zephyr/boards/espressif/esp32c6_devkitc/support/openocd.cfg -c 'tcl_port 6333' -c 'telnet_port 4444' -c 'gdb_port 3333' -c 'set ESP_RTOS Zephyr' -c 'init' -c 'reset halt' -c 'esp appimage_offset 0x0'
```

# 2. Deep Sleep topic

https://github.com/zephyrproject-rtos/zephyr/blob/18f4dfa9f58695e66599a63340098c32c29eaafa/samples/boards/espressif/deep_sleep/boards/esp32c6_devkitc_hpcore.overlay#L17

The ESP32-C6 has 7 dedicated LP GPIOs (numbered 0 through 6). These are the only pins capable of triggering a wake-up from deep sleep:

GPIO Number,Function,Notes
GPIO 0,LP_GPIO0,Often used for strapping; check your board schematic.
GPIO 1,LP_GPIO1,
GPIO 2,LP_GPIO2,
GPIO 3,LP_GPIO3,
GPIO 4,LP_GPIO4,
GPIO 5,LP_GPIO5,
GPIO 6,LP_GPIO6,

# 3. CANOpen topic

Tool to help parse the CANOpen *.eds file:

https://github.com/CANopenNode/CANopenEditor

CAN bus communication protocol of TMS/TMM61 sensors:

https://www.sick.com/media/docs/3/53/853/operating_instructions_tms_tmm61_tms_tmm88_tms_tmm88_dynamic_canopen_inclination_sensors_en_im0064853.pdf

## 3.1 Power up and intialization

After power up, the SICK sensor stays in pre-operational status,
until  Start Remote Node command switches the inclination sensor to Operational status.

It also automatically detects the baud rate on the CAN bus, 
it monitors the messages that are being sent and received on the CAN bus
but does not acknowledge them.

When a valid CAN telegram is received, the correct baud rate is identified and set. 
After this, the inclination sensor starts up, logs in with a boot-up message, and switches to Pre-Operational mode

Boot-up message
To signal that the device is ready for operation following switching on, a “boot-up
message” is sent out. This message uses the ID of the NMT error control protocol and
is permanently linked to the set device address (700h + node ID)

## 3.2 measurement data
By default, Transmit PDO, Transmission Type in object 1800.02h has value 1,
Synchronized data transmission

In synchronized data transmission, the process data is transmitted with the SYNC
messages. The cycle is formed from a multiple of the SYNC messages. The factor can
be between 1 and 240,

when the value is 1, every SYNC value will trigger a measurement and transmission.

## 3.3 operate the sensor

The process to power up and initialize the sensor is:

1. turn on power.
2. MCU repeately sends out RTR frame and monitor node guarding message or bootup message
3. MCU sends out "Start Remote Node" command
4. MCU repeately sends out  RTR frame and monitor node guarding message until in operational state
5. MCU sends out SYNC periodically, and gets SDO

struce device* can_dev
struct can_filter filter
can_rx_callback_t cb
int filter_id
void* user_data (sensor context)
struct k_msgq *	msgq //maybe message queue is a better idea