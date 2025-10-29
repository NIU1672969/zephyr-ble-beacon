/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <kernel.h>
#include <device.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <string.h>
#include <stdio.h>

#include <drivers/sensor.h>

//Include sensor gas
#include <drivers/i2c.h>
#include <logging/log.h>
#include <bluetooth/hci.h>

#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

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


//Sensor gas
LOG_MODULE_REGISTER(gas_sensor, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c0)
#define GAS_SENSOR_ADDR 0x04

/* Register map from Seeed/Arduino source */
#define GAS_CO_REG      0x02
#define GAS_NO2_REG     0x04
#define GAS_NH3_REG     0x06
#define GAS_CH4_REG     0x08
#define GAS_C2H5OH_REG  0x0A
#define GAS_H2_REG      0x0C   /* optional if supported */
#define GAS_PROPANE_REG 0x0E   /* optional if supported */

#define GAS_SCALE 100.0f  /* raw / scale => ppm */

static float read_gas(const struct device *i2c_dev, uint8_t reg)
{
    uint8_t buf[2];
    int ret = i2c_write_read(i2c_dev, GAS_SENSOR_ADDR, &reg, 1, buf, sizeof(buf));
    if (ret) {
        printk("I2C read failed reg 0x%02x (err %d)\n", reg, ret);
        return -1.0f;
    }
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    return (float)raw / GAS_SCALE;
}

static void read_all_gases(const struct device *i2c_dev)
{
    float co   = read_gas(i2c_dev, GAS_CO_REG);
    float no2  = read_gas(i2c_dev, GAS_NO2_REG);
    float nh3  = read_gas(i2c_dev, GAS_NH3_REG);
    float ch4  = read_gas(i2c_dev, GAS_CH4_REG);
    float etoh = read_gas(i2c_dev, GAS_C2H5OH_REG);

    /* Use integer print to avoid float formatting */
    uint16_t co_i   = (uint16_t)(co   * 100);
    uint16_t no2_i  = (uint16_t)(no2  * 100);
    uint16_t nh3_i  = (uint16_t)(nh3  * 100);
    uint16_t ch4_i  = (uint16_t)(ch4  * 100);
    uint16_t etoh_i = (uint16_t)(etoh * 100);

    printk("CO:%u.%02u NO2:%u.%02u NH3:%u.%02u CH4:%u.%02u C2H5OH:%u.%02u ppm\n",
           co_i/100,   co_i%100,
           no2_i/100,  no2_i%100,
           nh3_i/100,  nh3_i%100,
           ch4_i/100,  ch4_i%100,
           etoh_i/100, etoh_i%100);
}

void main(void)
{
    printk("Starting Multichannel Gas Sensor (register mode)\n");
    int err; printk("Starting Beacon Demo with DHT11\n");
     /* Initialize the Bluetooth Subsystem */ err = bt_enable(bt_ready); 
     if (err) { 
        printk("Bluetooth init failed (err %d)\n", err); 
    }

    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        printk("I2C device not ready\n");
        return;
    }

    k_sleep(K_SECONDS(1));  /* allow sensor MCU to boot */

    while (1) {
        read_all_gases(i2c_dev);
        k_sleep(K_SECONDS(2));
    }
}