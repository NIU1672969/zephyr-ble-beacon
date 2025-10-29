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

/* --- New Includes for GATT --- */
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/uuid.h>
/* ----------------------------- */


#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* --- Keep track of the current connection --- */
static struct bt_conn *current_conn;

/* --- Define Custom UUIDs for our Gas Sensor Service --- */

/* Gas Sensor Service UUID: 47617353-656e-736f-7253-766300000000 (ASCII: "GasSensorSvc") */
#define BT_UUID_GAS_SENSOR_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x47617353, 0x656e, 0x736f, 0x7253, 0x766300000000)
#define BT_UUID_GAS_SENSOR_SERVICE BT_UUID_DECLARE_128(BT_UUID_GAS_SENSOR_SERVICE_VAL)

/* Gas Readings Characteristic UUID: 47617352-6561-6469-6e67-73000000000 (ASCII: "GasReadings") */
#define BT_UUID_GAS_READINGS_CHAR_VAL \
	BT_UUID_128_ENCODE(0x47617352, 0x6561, 0x6469, 0x6e67, 0x73000000000)
#define BT_UUID_GAS_READINGS_CHAR BT_UUID_DECLARE_128(BT_UUID_GAS_READINGS_CHAR_VAL)

/* --- Buffer to hold our sensor data (5 floats * 4 bytes/float = 20 bytes) --- */
static uint8_t sensor_data_buffer[20];

/* --- GATT Service Callbacks --- */

/* Read callback for the characteristic */
static ssize_t read_gas_char_cb(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len,
			      uint16_t offset)
{
	/* Copy the current sensor data from our buffer to the client's buffer */
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 sensor_data_buffer, sizeof(sensor_data_buffer));
}

/* CCC (Client Characteristic Configuration) callback */
static void gas_char_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	/* Check if notifications are being enabled (BT_GATT_CCC_NOTIFY) */
	bool notifications_enabled = (value == BT_GATT_CCC_NOTIFY);
	printk("Notifications %s\n", notifications_enabled ? "enabled" : "disabled");
}

/* --- Define the GATT Service --- */
BT_GATT_SERVICE_DEFINE(gas_sensor_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GAS_SENSOR_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_GAS_READINGS_CHAR,
			       /* FIX 1: Changed BT_GATT_CH_... to BT_GATT_CHRC_... */
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, /* Properties: Read and Notify */
			       /* FIX 2: Changed BT_GATT_CSEC_... to BT_GATT_PERM_... */
			       BT_GATT_PERM_READ, /* Permissions: Readable */
			       read_gas_char_cb,  /* Read callback */
			       NULL,              /* Write callback (none) */
			       NULL),             /* User data (none) */
	/* CCC descriptor for notifications */
	BT_GATT_CCC(gas_char_ccc_cfg_changed,
		    /* FIX 3: Changed BT_GATT_CSEC_... to BT_GATT_PERM_... */
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE) /* Permissions: Read/Write */
);


/* --- New Advertising Data (connectable, advertises our service) --- */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	/* Advertise our 128-bit Gas Sensor Service UUID */
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_GAS_SENSOR_SERVICE_VAL)
};

/* Set Scan Response data (same as before) */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* --- Connection Callbacks --- */
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
		current_conn = bt_conn_ref(conn);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

/* Register the connection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
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

	/* Start advertising (CHANGED to connectable) */
	/* FIX 5: Replaced BT_LE_ADV_PARAM_DEFAULT with BT_LE_ADV_CONN */
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

	printk("Advertising started, connectable as %s\n", addr_s);
}


/* Sensor gas code (from your original file) */
LOG_MODULE_REGISTER(gas_sensor, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c0)
#define GAS_SENSOR_ADDR 0x04

/* Register map from Seeed/Arduino source */
#define GAS_CO_REG 	0x02
#define GAS_NO2_REG 	0x04
#define GAS_NH3_REG 	0x06
#define GAS_CH4_REG 	0x08
#define GAS_C2H5OH_REG 	0x0A

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

/* Helper function to copy a float into our byte buffer */
static void float_to_bytes(float f, uint8_t *buf)
{
	/* Assumes little-endian, which is correct for nRF52 */
	memcpy(buf, &f, sizeof(f));
}

static void read_all_gases(const struct device *i2c_dev)
{
	float co   = read_gas(i2c_dev, GAS_CO_REG);
	float no2  = read_gas(i2c_dev, GAS_NO2_REG);
	float nh3  = read_gas(i2c_dev, GAS_NH3_REG);
	float ch4  = read_gas(i2c_dev, GAS_CH4_REG);
	float etoh = read_gas(i2c_dev, GAS_C2H5OH_REG);

	/* Use integer print to avoid float formatting (same as your code) */
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

	/* --- NEW: Update the BLE characteristic buffer --- */
	float_to_bytes(co,   &sensor_data_buffer[0]);  /* Bytes 0-3 */
	float_to_bytes(no2,  &sensor_data_buffer[4]);  /* Bytes 4-7 */
	float_to_bytes(nh3,  &sensor_data_buffer[8]);  /* Bytes 8-11 */
	float_to_bytes(ch4,  &sensor_data_buffer[12]); /* Bytes 12-15 */
	float_to_bytes(etoh, &sensor_data_buffer[16]); /* Bytes 16-19 */

	/* --- NEW: Send notification if connected --- */
	if (current_conn) {
		bt_gatt_notify(current_conn,
			       &gas_sensor_service.attrs[1], /* The characteristic attribute */
			       sensor_data_buffer,
			       sizeof(sensor_data_buffer));
	}
}

void main(void)
{
	int err;
	printk("Starting Multichannel Gas Sensor (GATT Server mode)\n");

	/* --- NEW: Initialize Bluetooth --- */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	/* bt_ready() will be called when BLE stack is up */


	/* --- Your existing I2C init code --- */
	const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
	if (!device_is_ready(i2c_dev)) {
		printk("I2C device not ready\n");
		return;
	}

	k_sleep(K_SECONDS(1));   /* allow sensor MCU to boot */

	while (1) {
		read_all_gases(i2c_dev);
		k_sleep(K_SECONDS(2));
	}
}


