/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <device.h>

#include <drivers/sensor.h>

//Include sensor gas
#include <drivers/i2c.h>
#include <logging/log.h>

#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

//Sensor gas
LOG_MODULE_REGISTER(gas_sensor, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

//Sensor gas
#define I2C_NODE DT_NODELABEL(i2c0)
#define GAS_SENSOR_ADDR 0x04

/*
 * Set Advertisement data. Based on the Eddystone specification:
 * https://github.com/google/eddystone/blob/master/protocol-specification.md
 * https://github.com/google/eddystone/tree/master/eddystone-url
 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
		      0xaa, 0xfe, /* Eddystone UUID */
		      0x10, /* Eddystone-URL frame type */
		      0x00, /* Calibrated Tx power at 0m */
		      0x00, /* URL Scheme Prefix http://www. */
		      'z', 'e', 'p', 'h', 'y', 'r',
		      'p', 'r', 'o', 'j', 'e', 'c', 't',
		      0x08) /* .org */
};

/* Set Scan Response data */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void bt_ready(int err)
{
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}


	/* For connectable advertising you would use
	 * bt_le_oob_get_local().  For non-connectable non-identity
	 * advertising an non-resolvable private address is used;
	 * there is no API to retrieve that.
	 */

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

	printk("Beacon started, advertising as %s\n", addr_s);
}

static void read_sensor(void)
{
    const struct device *dht = DEVICE_DT_GET_ONE(aosong_dht);
    struct sensor_value temp, humidity;
    int ret;

    if (!dht) {
        printk("Error: Could not get DHT device\n");
        return;
    }

    if (!device_is_ready(dht)) {
        printk("Error: Device %s is not ready\n", dht->name);
        return;
    }

    printk("Attempting to read DHT11 sensor...\n");
    ret = sensor_sample_fetch(dht);
    if (ret < 0) {
        printk("Error: Sensor sample fetch failed (err %d)\n", ret);
        return;
    }

    ret = sensor_channel_get(dht, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret < 0) {
        printk("Error: Could not get temperature (err %d)\n", ret);
        return;
    }

    ret = sensor_channel_get(dht, SENSOR_CHAN_HUMIDITY, &humidity);
    if (ret < 0) {
        printk("Error: Could not get humidity (err %d)\n", ret);
        return;
    }

    printk("Temperature: %d.%d °C\n", temp.val1, temp.val2 / 100000);
    printk("Humidity: %d.%d %%\n", humidity.val1, humidity.val2 / 100000);
}

/* Read CO from the gas sensor over I2C.
 * NOTE: The register address and scaling below are placeholders —
 * check your sensor's datasheet and adjust GAS_CO_REG and CO_SCALE
 * accordingly. This implementation reads 2 bytes (MSB first) and
 * converts to a 16-bit value.
 */
#define GAS_CO_REG 0x02
#define CO_SCALE 100.0f /* placeholder: raw / CO_SCALE => ppm */

static void read_co_sensor(const struct device *i2c_dev)
{
    if (!i2c_dev) {
        printk("CO: invalid i2c device\n");
        return;
    }

    uint8_t reg = GAS_CO_REG;
    uint8_t buf[2];
    int ret;

    /* Write register address, then read 2 bytes */
    ret = i2c_write_read(i2c_dev, GAS_SENSOR_ADDR, &reg, 1, buf, sizeof(buf));
    if (ret) {
        printk("CO: read failed (err %d)\n", ret);
        return;
    }

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    float ppm = (float)raw / CO_SCALE;

    /* Opción 2: printk con enteros (para mostrar decimales sin float) */
    uint16_t ppm_int = (uint16_t)(ppm * 100); // 2 decimales
    printk("CO raw=0x%04x, approx=%u.%02u ppm\n",
           raw, ppm_int / 100, ppm_int % 100);
}

void main(void)
{
    int err;

    printk("Starting Beacon Demo with DHT11\n");

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
    }

    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    /* Per user request: remove the I2C scan and instead call the
     * CO-reading helper repeatedly. We intentionally don't abort if
     * the I2C device isn't ready here — the read_co_sensor function
     * will report errors. This keeps behavior simple and matches the
     * requested change.
     */
    printk("\n=== Starting CO readings (no I2C scan) ===\n");

    while (1) {
        read_co_sensor(i2c_dev);
        k_sleep(K_SECONDS(2));
    }
}
