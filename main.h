/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    struct Camera *camera;
    struct TimerUnit *timer;
} ThreadCreationArgs;

// Represents a timestamp from the GPS
typedef struct
{
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
    int milliseconds;
    bool locked;
    int remaining_exposure; // for current time
} TimerTimestamp;

// Represents an aquired frame
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t *data;
    double temperature;
    TimerTimestamp downloaded_time;
} CameraFrame;

void pn_log(const char * format, ...);
void queue_framedata(CameraFrame *frame);
void queue_trigger(TimerTimestamp *timestamp);
void clear_queued_data();

void trigger_fatal_error(char *message);
#endif

