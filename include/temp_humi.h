/* temp_humi.h - Public header for DHT11 temperature/humidity helper */
#ifndef TEMP_HUMI_H
#define TEMP_HUMI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Thread entry that sends temperature/humidity periodically.
 * The DHT11 devicetree alias/label needs to be defined in app.overlay.
 */
void dht11_notify_thread(void);

/* Simple helper used by the temp_humi module to notify the rest of the app
 * (for example to send a BLE notification). Implemented in `main.c`.
 */
int my_sensor_notify_string(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_HUMI_H */
