/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_SIMULATED_H
#define CAMERA_SIMULATED_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "camera.h"

void *camera_simulated_initialize(Camera *camera, ThreadCreationArgs *args);
double camera_simulated_update_camera_settings(Camera *camera, void *internal);
uint8_t camera_simulated_port_table(Camera *camera, void *internal, struct camera_port_option **ports);
void camera_simulated_uninitialize(Camera *camera, void *internal);
void camera_simulated_start_acquiring(Camera *camera, void *internal);
void camera_simulated_stop_acquiring(Camera *camera, void *internal);
void camera_simulated_tick(Camera *camera, void *internal, PNCameraMode current_mode);
double camera_simulated_read_temperature(Camera *camera, void *internal);
#endif