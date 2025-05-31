#ifndef __SENSORS_H
#define __SENSORS_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "state.h"
#include "telemetry.h"

#ifdef CONFIG_ADC_ADS1115
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>

#define N_ADC_CHANNELS 8

typedef struct {
    int channel_num;
    int sensor_id;
    telem_subtype_e type;
} adc_channel_t;

typedef struct {
    int id;
    int fd;
    const char *devpath;
    struct adc_msg_s sample[N_ADC_CHANNELS];
    int n_channels;
    adc_channel_t channels[4];
} adc_device_t;

int adc_read_value(adc_device_t *adc);
int adc_trigger_conversion(adc_device_t *adc);
int adc_sensor_val_conversion(adc_channel_t *channel, int32_t adc_val, int32_t *output_val);

#endif

#ifdef CONFIG_SENSORS_NAU7802
#include <uORB/uORB.h>

#ifndef DESKTOP_BUILD
#define SENSOR_MASS_KNOWN_WEIGHT CONFIG_HYSIM_PAD_SERVER_NAU7802_KNOWN_WEIGHT
#define SENSOR_MASS_KNOWN_POINT CONFIG_HYSIM_PAD_SERVER_NAU7802_KNOWN_POINT
#else
#ifndef SENSOR_MASS_KNOWN_WEIGHT
#define SENSOR_MASS_KNOWN_WEIGHT 1000
#endif

#ifndef SENSOR_MASS_KNOWN_POINT
#define SENSOR_MASS_KNOWN_POINT 1000
#endif

#endif
typedef struct {
    const struct orb_metadata *imu_meta;
    int imu;
    long zero_point;       /* Zero point of the sensor */
    long known_mass_grams; /* Calibration weight in grams */
    long known_mass_point; /* Calibration weight value */
    struct sensor_force data;
    bool available;
} sensor_mass_t;

int sensor_mass_init(sensor_mass_t *sensor_mass);
int sensor_mass_calibrate(sensor_mass_t *sensor_mass);
int sensor_mass_fetch(sensor_mass_t *sensor_mass);

#endif

#endif /* __SENSORS_H */
