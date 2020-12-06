/*
 *  Copyright (C) 2020 Mateusz Jończyk
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emax_em3371.h"
#include "main.h"
#include "psychrometrics.h"
#include "output_json.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/*
 * Using packed structs with casting may be unsafe on some architectures:
 * 	https://stackoverflow.com/questions/8568432/is-gccs-attribute-packed-pragma-pack-unsafe
 * Therefore I'm parsing the packet by hand.
 */

/*
 * Accesses three bytes from raw_data.
 * Returns true if at least some data is present.
 */
bool decode_single_measurement(struct device_single_measurement *measurement,
		const unsigned char *raw_data)
{
	if (raw_data[0] == 0xff && raw_data[1] == 0xff && raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		measurement->dew_point = DEVICE_INCORRECT_TEMPERATURE;
		return false;
	}

	if (raw_data[0] == 0xff && raw_data[1] == 0xff) {
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		fprintf(stderr, "Weird: measurement contains humidity, but not temperature.\n");
	} else {
		// Trying to be endianness-agnostic
		uint16_t raw_temperature = raw_data[0] + ((int)raw_data[1]) * 256;
		measurement->raw_temperature = raw_temperature;

		// The reverse engineering documentation contains the following formula for
		// calculating the real value of the temperature:
		// 	 f(x) = 0,0553127*x - 67,494980
		// The origin of both numbers is, however, not described. They may
		// have been obtained from lineal regression or something similar.
		//
		// It may be possible that the device sends the value in degrees Fahrenheit
		// multiplied by 10 with some offset.
		//
		// TODO: obtain more accurate formula
		measurement->temperature = 0.0553127 * (float)raw_temperature - 67.494980;
	}

	if (raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		fprintf(stderr, "Weird: measurement contains temperature but not humidity.\n");
	} else {
		measurement->humidity = raw_data[2];
	}

        // It would be more elegant to calculate the dew point in functions that
        // handle data presentation, but is done here for performance reasons.
        // It is a time-consuming calculation on devices with software floating
        // point emulation.
        if (measurement->humidity != DEVICE_INCORRECT_HUMIDITY &&
                !DEVICE_IS_INCORRECT_TEMPERATURE(measurement->temperature)) {

                measurement->dew_point =
                        dew_point(measurement->temperature, measurement->humidity);
        } else {
                measurement->dew_point = DEVICE_INCORRECT_TEMPERATURE;
        }

	return true;
}

bool decode_single_sensor_data(struct device_single_sensor_data *out,
		const unsigned char *raw_data)
{
	bool have_current_data = decode_single_measurement(&(out->current), raw_data);
	bool have_historical1_data = decode_single_measurement(&(out->historical1), raw_data + 3);
	bool have_historical2_data = decode_single_measurement(&(out->historical2), raw_data + 6);

	out->any_data_present = have_current_data || have_historical1_data || have_historical2_data;

	return out->any_data_present;
}

void decode_sensor_state(struct device_sensor_state *state, const unsigned char *received_packet,
		const size_t received_packet_size)
{
	if (received_packet_size <= 61) {
		fprintf(stderr, "Packet is too short!\n");
		return;
	}

	decode_single_sensor_data(&(state->station_sensor), received_packet + 22);
	for (int i = 0; i < 3; i++) {
		decode_single_sensor_data(&(state->remote_sensors[i]), received_packet + 31 + i*9);
	}

	if (received_packet[59] == 0xff && received_packet[60] == 0xff) {
		state->atmospheric_pressure = DEVICE_INCORRECT_PRESSURE;
	} else {
		state->atmospheric_pressure = received_packet[59] + received_packet[60]*256;
	}
}


void display_CSV_header(FILE *stream)
{
        fputs("time;atmospheric_pressure;"
                "station_temp;station_temp_raw;station_humidity;station_dew_point;"
                "sensor1_temp;sensor1_temp_raw;sensor1_humidity;sensor1_dew_point;"
                "sensor2_temp;sensor2_temp_raw;sensor2_humidity;sensor2_dew_point;"
                "sensor3_temp;sensor3_temp_raw;sensor3_humidity;sensor3_dew_point;"
                "\n", stream);
}

void display_single_measurement_CSV(FILE *stream, const struct device_single_measurement *state)
{
        if (DEVICE_IS_INCORRECT_TEMPERATURE(state->temperature)) {
                fprintf(stream, ";;");
        } else {
		fprintf(stream, "%.2f;%d;",
			(double) state->temperature, (int) state->raw_temperature
                        );
        }

        if (state->humidity == DEVICE_INCORRECT_HUMIDITY) {
                fprintf(stream, ";");
        } else {
		fprintf(stream, "%d;", (int) state->humidity);
        }

        if (DEVICE_IS_INCORRECT_TEMPERATURE(state->dew_point)) {
                fprintf(stream, ";");
        } else {
		fprintf(stream, "%.2f;", (double) state->dew_point);
        }
}

void display_sensor_state_CSV(FILE *stream, const struct device_sensor_state *state)
{
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time));

        fprintf(stream, "%s;%d;", current_time, state->atmospheric_pressure);
        display_single_measurement_CSV(stream, &(state->station_sensor.current));

        for (int i=0; i < 3; i++) {
                display_single_measurement_CSV(stream, &(state->remote_sensors[i].current));
        }

        fprintf(stream, "\n");
}

void init_logging()
{
        if (nan("") == 0 || !isnan(nan(""))) {
                //Are there any embedded architectures without support for NaN?
                //See:
                //      https://stackoverflow.com/q/2234468
                //      https://www.gnu.org/software/libc/manual/html_node/Infinity-and-NaN.html
                //              [...] available only on machines that support the “not a number”
                //              value—that is to say, on all machines that support IEEE floating
                //              point.
                fprintf(stderr, "ERROR: no support for NaN floating point numbers "
                                "in hardware or software floating point library.\n");
                exit(1);
        }

        fprintf(stderr, "Warning: output formats are subject to change\n");
        display_CSV_header(stdout);
}


// Main program logic
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size,
                const struct program_options *options)
{
	dump_incoming_packet(stderr, packet_source, received_packet, received_packet_size);

	if (received_packet_size < 20) {
                if (options->reply_to_ping_packets) {
                        reply_to_ping_packet(udp_socket, packet_source,
                                        received_packet, received_packet_size);
                }
	} else if (received_packet_size >= 65) {

		struct device_sensor_state *sensor_state;
		sensor_state = malloc(sizeof(struct device_sensor_state));
                if (sensor_state == NULL){
		        fprintf(stderr, "process_incoming_packet: Cannot allocate memory!\n");
                        return;
                }

		decode_sensor_state(sensor_state, received_packet, received_packet_size);
		display_sensor_state_json(stderr, sensor_state);
		display_sensor_state_CSV(stdout, sensor_state);

		free(sensor_state);
	}
}
