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