/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include "common.h"
#include "camera.h"
#include "timer.h"
#include "preferences.h"
#include "imagehandler.h"

#include "gui.h"
#include "platform.h"

struct PNFrameQueue
{
    PNFrame *frame;
    struct PNFrameQueue *next;
};

struct TimerTimestampQueue
{
    TimerTimestamp timestamp;
    struct TimerTimestampQueue *next;
};

pthread_mutex_t log_mutex, frame_queue_mutex, trigger_timestamp_queue_mutex;
FILE *logFile;
PNCamera *camera;
TimerUnit *timer;
struct PNFrameQueue *frame_queue = NULL;
struct TimerTimestampQueue *trigger_timestamp_queue;
char *fatal_error = NULL;

// Take a copy of the framedata and store it for
// processing on the main thread
void queue_framedata(PNFrame *frame)
{
    PNFrame *copy = malloc(sizeof(PNFrame));
    struct PNFrameQueue *tail = malloc(sizeof(struct PNFrameQueue));
    if (!copy || !tail)
    {
        trigger_fatal_error("Allocation error in queue_framedata");
        return;
    }

    // Add to frame queue
    memcpy(copy, frame, sizeof(PNFrame));
    tail->frame = copy;
    tail->next = NULL;

    pthread_mutex_lock(&frame_queue_mutex);
    // Empty queue
    if (frame_queue == NULL)
        frame_queue = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct PNFrameQueue *item = frame_queue;
        while (item->next != NULL)
            item = item->next;
        item->next = tail;
    }
    pthread_mutex_unlock(&frame_queue_mutex);
}

void process_framedata(PNFrame *frame)
{
    // Pop the head frame from the queue
    TimerTimestamp timestamp = (TimerTimestamp) {.valid = false};
    if (trigger_timestamp_queue != NULL)
    {
        pthread_mutex_lock(&trigger_timestamp_queue_mutex);
        struct TimerTimestampQueue *head = trigger_timestamp_queue;
        trigger_timestamp_queue = trigger_timestamp_queue->next;
        pthread_mutex_unlock(&trigger_timestamp_queue_mutex);
        timestamp = head->timestamp;
    }

    pn_log("Frame downloaded");

    // Display the frame in ds9
    pn_save_preview(frame, timestamp);
    pn_run_preview_script("preview.fits.gz");

    if (pn_preference_char(SAVE_FRAMES))
    {
        const char *filename = pn_save_frame(frame, timestamp, camera);
        if (filename == NULL)
        {
            pn_log("Save failed. Discarding frame");
            return;
        }

        pn_preference_increment_framecount();
        pn_run_saved_script(filename);
        free((char *)filename);
    }
}

/*
 * Add a timestamp to the trigger timestamp queue
 */
void queue_trigger_timestamp(TimerTimestamp timestamp)
{
    struct TimerTimestampQueue *tail = malloc(sizeof(struct TimerTimestampQueue));
    if (tail == NULL)
        trigger_fatal_error("Unexpected download - no timestamp available for frame");

    tail->timestamp = timestamp;
    tail->next = NULL;

    pthread_mutex_lock(&trigger_timestamp_queue_mutex);
    size_t count = 0;
    // Empty queue
    if (trigger_timestamp_queue == NULL)
        trigger_timestamp_queue = tail;
    else
    {
        // Find tail of queue - queue is assumed to be short
        struct TimerTimestampQueue *item = trigger_timestamp_queue;
        while (item->next != NULL)
        {
            item = item->next;
            count++;
        }
        item->next = tail;
    }
    count++;
    pthread_mutex_unlock(&trigger_timestamp_queue_mutex);
    pn_log("Pushed timestamp. %d in queue", count);
}

void trigger_fatal_error(char *message)
{
    fatal_error = message;
}

int main(int argc, char *argv[])
{
    // Parse the commandline args
    bool simulate_camera = false;
    bool simulate_timer = false;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--simulate-camera") == 0)
            simulate_camera = true;

        if (strcmp(argv[i], "--simulate-timer") == 0)
            simulate_timer = true;
    }

    // Initialization
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&frame_queue_mutex, NULL);
    pthread_mutex_init(&trigger_timestamp_queue_mutex, NULL);

    // Open the log file for writing
    time_t start = time(NULL);
    char namebuf[32];
    strftime(namebuf, 32, "logs/%Y%m%d-%H%M%S.log", gmtime(&start));
    logFile = fopen(namebuf, "w");
    if (logFile == NULL)
    {
        fprintf(stderr, "Unable to create logfile %s\n", namebuf);
        exit(1);
    }

    pn_init_preferences("preferences.dat");
    timer = timer_new(simulate_timer);
    camera = pn_camera_new(simulate_camera);

    // Start ui early so it can catch log events
    pn_ui_new(camera, timer);
    pn_run_startup_script();

    ThreadCreationArgs args;
    args.camera = camera;
    args.timer = timer;

    timer_spawn_thread(timer, &args);
    pn_camera_spawn_thread(camera, &args);

    // Main program loop
    for (;;)
    {
        if (fatal_error)
        {
            pn_ui_show_fatal_error(fatal_error);
            break;
        }

        // Process stored frames
        struct PNFrameQueue *head = NULL;
        do
        {
            pthread_mutex_lock(&frame_queue_mutex);
            head = frame_queue;

            // No frames to process
            if (head == NULL)
            {
                pthread_mutex_unlock(&frame_queue_mutex);
                break;
            }

            // Pop the head frame
            frame_queue = frame_queue->next;
            pthread_mutex_unlock(&frame_queue_mutex);

            PNFrame *frame = head->frame;
            free(head);
            process_framedata(frame);
            free(frame);
        } while (head != NULL);

        bool request_shutdown = pn_ui_update();
        if (request_shutdown)
            break;
        
        millisleep(100);
    }

    // Wait for camera and timer threads to terminate
    pn_camera_shutdown(camera);
    timer_shutdown(timer);

    // Final cleanup
    if (fatal_error)
        free(fatal_error);

    // Other threads have closed, so don't worry about locking
    while (frame_queue != NULL)
    {
        struct PNFrameQueue *next = frame_queue->next;
        free(frame_queue->frame);
        free(frame_queue);
        frame_queue = next;
        pn_log("Discarding queued framedata");
    }

    while (trigger_timestamp_queue != NULL)
    {
        struct TimerTimestampQueue *next = trigger_timestamp_queue->next;
        free(trigger_timestamp_queue);
        trigger_timestamp_queue = next;
        pn_log("Discarding queued timestamp");
    }

    timer_free(timer);
    pn_camera_free(camera);
    pn_free_preferences();
    pn_ui_free();
    fclose(logFile);

    pthread_mutex_destroy(&trigger_timestamp_queue_mutex);
    pthread_mutex_destroy(&frame_queue_mutex);
    pthread_mutex_destroy(&log_mutex);

    return 0;
}

// Add a message to the gui and saved log files
void pn_log(const char * format, ...)
{
    va_list args;
    va_start(args, format);

    // Log time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    struct tm *ptm = gmtime(&seconds);
    char timebuf[9];
    strftime(timebuf, 9, "%H:%M:%S", ptm);

    pthread_mutex_lock(&log_mutex);

    // Construct log line
    char *msgbuf, *linebuf;
    vasprintf(&msgbuf, format, args);
    asprintf(&linebuf, "[%s.%03d] %s", timebuf, (int)(tv.tv_usec / 1000), msgbuf);
    free(msgbuf);

    // Log to file
    fprintf(logFile, "%s", linebuf);
    fprintf(logFile, "\n");

    // Add to gui
    pn_ui_log_line(linebuf);

    pthread_mutex_unlock(&log_mutex);

    free(linebuf);
    va_end(args);
}
