
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include "temp_humi.h"



/* Alias we defined in the overlay */
#define DHT11_NODE DT_ALIAS(dht11)

#if !DT_NODE_HAS_STATUS(DHT11_NODE, okay)
#error "DHT11 devicetree alias is not defined or status is not 'okay'"
#endif

/* Thread function to send sensor data via notification periodically */
void dht11_notify_thread(void)
{
    const struct device *dht = DEVICE_DT_GET(DHT11_NODE);

    if (!device_is_ready(dht)) {
        printk("DHT11 device not ready\n");
        return;
    }

    struct sensor_value temp, hum;
    char data_str[20];  // Enough for "100C | 100%\0"

    while (1) {
        if (sensor_sample_fetch(dht) == 0) 
        {
            sensor_channel_get(dht, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            sensor_channel_get(dht, SENSOR_CHAN_HUMIDITY, &hum);

            snprintf(data_str, sizeof(data_str), "%dC | %d%%", temp.val1, hum.val1);
            printk("Sending: %s\n", data_str);

            int err = my_sensor_notify_string(data_str);
            if (err == -EACCES) {
                printk("Notification not enabled by central.\n");
            } else if (err) {
                printk("Notification sent!\n");
            }
        } 
        else 
        {
            printk("Sample fetch failed\n");
        }

        k_sleep(K_MSEC(2000));
    }
}