#include "sensors.h"
#include <math.h>

/*
 * Maps a value in the input range to the output range.
 * @param value The value to map
 * @param in_min The minimum value of the input range
 * @param in_max The maximum value of the input range
 * @param out_min The minimum value of the output range
 * @param out_max The maximum value of the output range
 * @return The newly mapped value
 */
static double map_value(double value, double in_min, double in_max, double out_min, double out_max) {
    double slope = (out_max - out_min) / (in_max - in_min);
    if (slope == 0) return 0;
    return out_min + slope * (value - in_min);
}

#ifdef CONFIG_ADC_ADS1115
/*
 * A function to trigger ADC conversion
 * @param adc The ADC device structure
 * @return 0 for success, error code on failure
 */
int adc_trigger_conversion(adc_device_t *adc) { return ioctl(adc->fd, ANIOC_TRIGGER, 0); }

/*
 * A function to read ADC value after conversion
 * @param adc The ADC device structure
 * @return 0 for success, error code on failure
 */
int adc_read_value(adc_device_t *adc) {
    ssize_t nbytes = read(adc->fd, adc->sample, sizeof(adc->sample));
    if (nbytes < 0) {
        fprintf(stderr, "Failed to read ADC value\n");
        return nbytes;
    } else if (nbytes == 0) {
        printf("No data read from ADC\n");
        return -1;
    }
    return OK;
}

#endif

#ifdef CONFIG_SENSORS_MCP9600

int sensor_temp_init(sensor_temp_t *sensor_temp, char *dev) {
    sensor_temp->dev = dev;
    sensor_temp->imu_meta = orb_get_meta(dev);

    if (sensor_temp->imu_meta == NULL) {
        return -1;
    }

    sensor_temp->imu = orb_subscribe(sensor_temp->imu_meta);
    if (sensor_temp->imu < 0) {
        return -1;
    }

    sensor_temp->imu = open(dev, O_RDWR);
    if (sensor_temp->imu < 0) {
        return -1;
    }

    return 0;
}

/* A funcion to fetch the ambient temperature data
 * @param The sensor temp object
 * @param data The data to be fetched
 * @return 0 for success, error code on failure
 */
int sensor_temp_fetch(sensor_temp_t *sensor_temp) {
    int err = 0;
    bool update = false;
    err = orb_check(sensor_temp->imu, &update);
    if (err < 0) {
        return err;
    }

    return orb_copy(sensor_temp->imu_meta, sensor_temp->imu, &(sensor_temp->data));
}

#endif

#ifdef CONFIG_SENSORS_NAU7802
/* A funcion to fetch the sensor mass data
 * @param sensor_mass The sensor mass object
 * @param data The data to be fetched
 * @return 0 for success, error code on failure
 */
int sensor_mass_fetch(sensor_mass_t *sensor_mass) {
    int err = 0;
    bool update = false;
    err = orb_check(sensor_mass->imu, &update);
    if (err < 0) {
        return err;
    }

    return orb_copy(sensor_mass->imu_meta, sensor_mass->imu, &(sensor_mass->data));
}

/* A function to initialize the mass sensor with UORB
 * @param sensor_mass The sensor mass object
 * @return 0 for success, error code on failure
 */
int sensor_mass_init(sensor_mass_t *sensor_mass) {
    char *name = "sensor_force0";

    sensor_mass->imu_meta = orb_get_meta(name);

    if (sensor_mass->imu_meta == NULL) {
        return -1;
    }

    sensor_mass->imu = orb_subscribe(sensor_mass->imu_meta);
    if (sensor_mass->imu < 0) {
        return -1;
    }

    return 0;
}

/* A function to calibrate the mass sensor zero point
 * @param sensor_mass The sensor mass object
 * @return 0 for success, error code on failure
 */
int sensor_mass_calibrate(sensor_mass_t *sensor_mass) {
    int err = 0;

    /* Flush 10 readings */
    for (int i = 0; i < 10; i++) {
        err = sensor_mass_fetch(sensor_mass);
        if (err < 0) {
            i--;
        }
        usleep(100000);
    }

    /* Get the zero point */
    sensor_mass->zero_point = 0;
    for (int i = 0; i < 10; i++) {
        err = sensor_mass_fetch(sensor_mass);
        if (err < 0) {
            i--;
        } else {
            sensor_mass->zero_point += sensor_mass->data.force / 10;
        }
        usleep(100000);
    }

    return 0;
}
#endif /* CONFIG_SENSOR_NAU7802 */

#ifdef CONFIG_ADC_ADS1115

/*
 * Convert ADC voltage value to the corresponding measurement value of the sensor.
 * @param channel The ADC channel being measured
 * @param adc_val The value read by the ADC channel
 * @param output_val The outcome of the measurement after calculation
 */
int adc_sensor_val_conversion(adc_channel_t *channel, int32_t adc_val, int32_t *output_val) {

    /* 6.144 is the FSR of the ADC at PGA value 0 */

    double sensor_voltage = ((double)adc_val * 6.144) / (32768.0);

    switch (channel->type) {

    case TELEM_PRESSURE: {
        double val_max;
        if (channel->sensor_id == 4 || channel->sensor_id == 5) {
            val_max = 1000.0;
        } else {
            val_max = 2500.0;
        }
        *output_val = map_value(sensor_voltage, 1.0, 5.0, 0.0, val_max);
    } break;

    case TELEM_MASS: {
        /* 0 - 2,500lbs according to Antoine */
        *output_val = map_value(sensor_voltage, 0.0, 5.0, 0.0, 2500.0);
    } break;

    case TELEM_CONT: {
        if (sensor_voltage <= 1) { /* Threshold voltage to switch state*/
            *output_val = 0;
        } else {
            *output_val = 1;
        }
    } break;

    case TELEM_TEMP: {
        /* If you're wondering what is this I don't know either, it was pulled from the old code */

        double A, B, C;
        double R, T;

        /* Set coefficients based on sensor_id */
        if (channel->sensor_id == 0) { /* Thermistor 1 */
            A = 1.403 * 0.001;
            B = 2.373 * 0.0001;
            C = 9.827 * 0.00000001;
        } else { /* Thermistor 2 */
            A = 1.468 * 0.001;
            B = 2.383 * 0.0001;
            C = 1.007 * 0.0000001;
        }

        if (sensor_voltage > 0) {
            R = 2948.0 / ((4.945 / sensor_voltage) - 1.0);

            if (R > 0) {
                T = 1.0 / (A + B * log(R) + C * pow(log(R), 3));
                *output_val = (int32_t)((T - 273.15) * 100);
            } else {
                *output_val = 0;
            }
        } else {
            *output_val = 0;
        }
    } break;
    }
    return 0;
}
#endif
