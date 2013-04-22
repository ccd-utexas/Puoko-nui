/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_SIMULATED_H
#define CAMERA_SIMULATED_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "camera.h"

int camera_simulated_initialize(Camera *camera, void **internal);
int camera_simulated_update_camera_settings(Camera *camera, void *internal, double *readout_time);
int camera_simulated_port_table(Camera *camera, void *internal, struct camera_port_option **ports, uint8_t *port_count);
int camera_simulated_uninitialize(Camera *camera, void *internal);
int camera_simulated_start_acquiring(Camera *camera, void *internal, bool shutter_open);
int camera_simulated_stop_acquiring(Camera *camera, void *internal);
int camera_simulated_tick(Camera *camera, void *internal, PNCameraMode current_mode);
int camera_simulated_read_temperature(Camera *camera, void *internal, double *temperature);
int camera_simulated_query_ccd_region(Camera *camera, void *internal, uint16_t region[4]);

bool camera_simulated_supports_readout_display(Camera *camera, void *internal);
void camera_simulated_normalize_trigger(Camera *camera, void *internal, TimerTimestamp *trigger);

void camera_simulated_trigger_frame(Camera *camera, void *internal);
#endif