#include "sensors.h"

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

int adc_sensor_val_conversion(adc_channel_t *channel, int32_t adc_val, int32_t *output_val) {
    /* 6.144 is the FSR of the ADC at PGA value 0 */
    double sensor_voltage = ((double)adc_val * 6.144) / (32768.0);

    if (sensor_voltage < channel->v_min || sensor_voltage > channel->v_max) {
        return -1;
    }

    if (channel->type == TELEM_PRESSURE || channel->type == TELEM_MASS) {
        double slope = (double)(channel->v_max - channel->v_min) / (double)(channel->val_max - channel->val_min);
        if (slope == 0) {
            return -1;
        }
        double y_intercept = channel->v_min - slope * channel->val_min;
        *output_val = (sensor_voltage - y_intercept) / slope;

    } else if (channel->type == TELEM_CONT) {
        if (sensor_voltage <= 1) { /* Threshold voltage to switch state*/
            *output_val = 0;
        } else {
            *output_val = 1;
        }
    } else if (channel->type == TELEM_TEMP) {
        *output_val = sensor_voltage;
    } else {
        return -1;
    }

    return 0;
}
#endif
