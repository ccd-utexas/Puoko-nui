/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef CAMERA_PVCAM_H
#define CAMERA_PVCAM_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "camera.h"

void *camera_pvcam_initialize(Camera *camera, ThreadCreationArgs *args);
double camera_pvcam_update_camera_settings(Camera *camera, void *internal);
uint8_t camera_pvcam_port_table(Camera *camera, void *internal, struct camera_port_option **ports);
void camera_pvcam_uninitialize(Camera *camera, void *internal);
void camera_pvcam_start_acquiring(Camera *camera, void *internal);
void camera_pvcam_stop_acquiring(Camera *camera, void *internal);
void camera_pvcam_tick(Camera *camera, void *internal, PNCameraMode current_mode, double current_temperature);
double camera_pvcam_read_temperature(Camera *camera, void *internal);
void camera_pvcam_query_ccd_region(Camera *camera, void *internal, uint16_t region[4]);

#endif
