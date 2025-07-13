#include <math.h>

#include "../../debugging/logging.h"
#include "sensors.h"

#if defined CONFIG_SENSORS_MCP9600
#include <nuttx/sensors/mcp9600.h>
#endif

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

#ifdef CONFIG_SENSORS_MCP9600

int sensor_temp_init(sensor_temp_t *sensor_temp) {
    char *topic_name = "sensor_temp";
    sensor_temp->orb_meta = orb_get_meta(topic_name);

    if (sensor_temp->orb_meta == NULL) {
        return -1;
    }

    sensor_temp->fd = orb_subscribe_multi(sensor_temp->orb_meta, sensor_temp->topic);
    if (sensor_temp->fd < 0) {
        return -1;
    }

    // Set the thermocouple to be type K
    int err = ioctl(sensor_temp->fd, SNIOC_SET_THERMO, SENSOR_THERMO_TYPE_K);
    if (err < 0) {
        return -1;
    }

    return 0;
}

/* A funcion to fetch the temperature data
 * @param The sensor temp object
 * @param data The data to be fetched
 * @return 0 for success, error code on failure
 */
int sensor_temp_fetch(sensor_temp_t *sensor_temp) {
    int err = 0;
    bool update = false;
    err = orb_check(sensor_temp->fd, &update);
    if (err < 0) {
        return err;
    }

    if (update) {
        return orb_copy(sensor_temp->orb_meta, sensor_temp->fd, &(sensor_temp->data));
    }

    return -1;
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
    err = orb_check(sensor_mass->fd, &update);
    if (err < 0) {
        return err;
    }

    if (update) {
        return orb_copy(sensor_mass->orb_meta, sensor_mass->fd, &(sensor_mass->data));
    }

    return -1;
}

/* A function to initialize the mass sensor with UORB
 * @param sensor_mass The sensor mass object
 * @return 0 for success, error code on failure
 */
int sensor_mass_init(sensor_mass_t *sensor_mass) {
    char *name = "sensor_force0";

    sensor_mass->orb_meta = orb_get_meta(name);

    if (sensor_mass->orb_meta == NULL) {
        return -1;
    }

    sensor_mass->fd = orb_subscribe(sensor_mass->orb_meta);
    if (sensor_mass->fd < 0) {
        return -1;
    }

    int err = orb_set_interval(sensor_mass->fd, 50000);
    if (err < 0) {
        return -1;
    }

    FAR unsigned mass_frequency;
    err = orb_get_frequency(sensor_mass->fd, &mass_frequency);
    if (err < 0) {
        return -1;
    }
    hinfo("Mass sensor frequency set at %dHz\n", mass_frequency);

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
    hinfo("Channel #%u Voltage: %.2fV\n", channel->channel_num, sensor_voltage);

    switch (channel->type) {

    case TELEM_PRESSURE: {
        if (sensor_voltage < 1.0) {
            *output_val = 0;
            break;
        }

        *output_val = 1000 * map_value(sensor_voltage, 1.0, 5.0, 0.0, 1000.0);
        hinfo("Pressure #%d: %ld mPSI\n", channel->sensor_id, *output_val);
    } break;

    case TELEM_THRUST: {
        /* 0 - 2,500lbs according to Antoine, using values in Newtons */
        if (sensor_voltage < 0) {
            *output_val = 0;
            break;
        }
        *output_val = map_value(sensor_voltage, 0, 5.053, 0.0, 11120.5);
        hinfo("Mass #%d: %ld N\n", channel->sensor_id, *output_val);
    } break;

    case TELEM_CONT: {
        if (sensor_voltage <= 1) { /* Threshold voltage to switch state*/
            *output_val = 1;
        } else {
            *output_val = 0;
        }
        hinfo("Continuity: '%s'\n", *output_val ? "open circuit" : "continuous");
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
        hinfo("Temperature #%d: %ld mC\n", channel->sensor_id, *output_val);
    } break;
    }
    return 0;
}
#endif
