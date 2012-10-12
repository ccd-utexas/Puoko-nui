/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <picam.h>
#include <picam_advanced.h>

#include "main.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"

struct internal
{
    PicamHandle device_handle;
    PicamHandle model_handle;
    pibyte *image_buffer;

    uint16_t frame_width;
    uint16_t frame_height;
};

static void fatal_error(struct internal *internal, char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char *err = malloc((len + 1)*sizeof(char));
    if (err)
    {
        va_start(args, format);
        vsnprintf(err, len + 1, format, args);
        va_end(args);
    }
    else
        pn_log("Failed to allocate memory for fatal error.");

    trigger_fatal_error(err);

    // Attempt to die cleanly
    Picam_StopAcquisition(internal->model_handle);
    PicamAdvanced_CloseCameraDevice(internal->device_handle);
    Picam_UninitializeLibrary();
    pthread_exit(NULL);
}

static void print_error(const char *msg, PicamError error)
{
    if (error == PicamError_None)
        return;

    const pichar* string;
    Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &string);
    pn_log("%s: %s.", msg, string);
    Picam_DestroyString(string);
}

static void set_integer_param(PicamHandle model_handle, PicamParameter parameter, piint value)
{
    PicamError error = Picam_SetParameterIntegerValue(model_handle, parameter, value);
    if (error != PicamError_None)
    {
        const pichar *name, *err;
        Picam_GetEnumerationString(PicamEnumeratedType_Parameter, parameter, &name);
        Picam_GetEnumerationString(PicamEnumeratedType_Error, error, &err);
        pn_log("Failed to set `%s': %s.", name, err);
        Picam_DestroyString(err);
        Picam_DestroyString(name);
    }
}

static void commit_camera_params(struct internal *internal)
{
    pibln all_params_committed;
    Picam_AreParametersCommitted(internal->model_handle, &all_params_committed);

    if (!all_params_committed)
    {
        const PicamParameter *failed_params = NULL;
        piint failed_param_count = 0;
        PicamError error = Picam_CommitParameters(internal->model_handle, &failed_params, &failed_param_count);
        if (error != PicamError_None)
            print_error("Picam_CommitParameters failed.", error);

        if (failed_param_count > 0)
        {
            pn_log("%d parameters failed to commit:", failed_param_count);
            for (piint i = 0; i < failed_param_count; i++)
            {
                const pichar *name;
                Picam_GetEnumerationString(PicamEnumeratedType_Parameter, failed_params[i], &name);
                pn_log("   %s", name);
                Picam_DestroyString(name);
            }
            Picam_DestroyParameters(failed_params);
            fatal_error(internal, "Parameter commit failed.");
        }
        Picam_DestroyParameters(failed_params);

        if (PicamAdvanced_CommitParametersToCameraDevice(internal->model_handle) != PicamError_None)
            pn_log("Advanced parameter commit failed.");
    }
}

static double read_temperature(PicamHandle model_handle)
{
    piflt temperature;
    PicamError error = Picam_ReadParameterFloatingPointValue(model_handle, PicamParameter_SensorTemperatureReading, &temperature);
    if (error != PicamError_None)
        print_error("Temperature Read failed", error);

    // TODO: Can query PicamEnumeratedType_SensorTemperatureStatus to get locked/unlocked status
    return temperature;
}

// Frame status change callback
// - called when any of the following occur during acquisition:
//   - a new readout arrives
//   - acquisition completes due to total readouts acquired
//   - acquisition completes due to a stop request
//   - acquisition completes due to an acquisition error
//   - an acquisition error occurs
// - called on another thread
// - all update callbacks are serialized
static struct internal *callback_internal_ref;
PicamError PIL_CALL acquisitionUpdatedCallback(PicamHandle handle, const PicamAvailableData* data,
                                                      const PicamAcquisitionStatus* status)
{
    if (data && data->readout_count && status->errors == PicamAcquisitionErrorsMask_None)
    {
        // Copy frame data and pass ownership to main thread
        CameraFrame *frame = malloc(sizeof(CameraFrame));
        if (frame)
        {
            size_t frame_bytes = callback_internal_ref->frame_width*callback_internal_ref->frame_height*sizeof(uint16_t);
            frame->data = malloc(frame_bytes);
            if (frame->data)
            {
                memcpy(frame->data, data->initial_readout, frame_bytes);
                frame->width = callback_internal_ref->frame_width;
                frame->height = callback_internal_ref->frame_height;
                frame->temperature = read_temperature(callback_internal_ref->model_handle);
                queue_framedata(frame);
            }
            else
                pn_log("Failed to allocate CameraFrame->data. Discarding frame.");
        }
        else
            pn_log("Failed to allocate CameraFrame. Discarding frame.");
    }

    // Error
    else if (status->errors != PicamAcquisitionErrorsMask_None)
    {
        // Print errors
        if (status->errors & PicamAcquisitionErrorsMask_DataLost)
            pn_log("Frame data lost. Continuing.");

        if (status->errors & PicamAcquisitionErrorsMask_ConnectionLost)
            pn_log("Camera connection lost. Continuing.");
    }

    // Check for buffer overrun. Should never happen in practice, but we log this
    // to aid future debugging if it does crop up in the field.
    pibln overran;
    PicamError error = PicamAdvanced_HasAcquisitionBufferOverrun(handle, &overran);
    if (error != PicamError_None)
        print_error("Failed to check for acquisition overflow. Continuing.", error);
    else if (overran)
        pn_log("Acquisition buffer overflow! Continuing.");

    return PicamError_None;
}

// Connect to the first available camera
// Expects PICAM to be initialized
static void connect_camera(Camera *camera, struct internal *internal)
{
    // Loop until a camera is available or the user gives up
    while (camera_desired_mode(camera) != SHUTDOWN)
    {
        const PicamCameraID *cameras = NULL;
        piint camera_count = 0;

        PicamError error = Picam_GetAvailableCameraIDs(&cameras, &camera_count);
        if (error != PicamError_None || camera_count == 0)
        {
            pn_log("Waiting for camera...");

            // Camera detection fails unless we close and initialize the library
            Picam_UninitializeLibrary();
            Picam_InitializeLibrary();

            // Wait a bit longer so we don't spin and eat CPU
            millisleep(500);

            continue;
        }

        error = PicamAdvanced_OpenCameraDevice(&cameras[0], &internal->device_handle);
        if (error != PicamError_None)
        {
            print_error("PicamAdvanced_OpenCameraDevice failed.", error);
            continue;
        }

        error = PicamAdvanced_GetCameraModel(internal->device_handle, &internal->model_handle);
        if (error != PicamError_None)
        {
            print_error("Failed to query camera model.", error);
            continue;
        }

        return;
    }
}

void *camera_picam_initialize(Camera *camera, ThreadCreationArgs *args)
{
    struct internal *internal = calloc(1, sizeof(struct internal));
    if (!internal)
        return NULL;

    Picam_InitializeLibrary();

    connect_camera(camera, internal);

    // User has given up waiting for a camera
    if (camera_desired_mode(camera) == SHUTDOWN)
        return NULL;

    PicamCameraID id;
    Picam_GetCameraID(internal->device_handle, &id);

    // Query camera model info
    const pichar *string;
    Picam_GetEnumerationString(PicamEnumeratedType_Model, id.model, &string);
    pn_log("Camera ID: %s (SN:%s) [%s].", string, id.serial_number, id.sensor_name);
    Picam_DestroyString(string);
    Picam_DestroyCameraIDs(&id);

    // Enable frame transfer mode
    set_integer_param(internal->model_handle, PicamParameter_ReadoutControlMode, PicamReadoutControlMode_FrameTransfer);

    // Enable external trigger
    set_integer_param(internal->model_handle, PicamParameter_TriggerResponse, PicamTriggerResponse_ReadoutPerTrigger);

    // Set falling edge trigger (actually low level trigger)
    set_integer_param(internal->model_handle, PicamParameter_TriggerDetermination, PicamTriggerDetermination_FallingEdge);

    // Set output high when the camera is able to respond to a readout trigger
    set_integer_param(internal->model_handle, PicamParameter_OutputSignal, PicamOutputSignal_WaitingForTrigger);

    // Keep the shutter closed until we start a sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);

    // Continue exposing until explicitly stopped or error
    // Requires a user specified image buffer to be provided - the interal
    // routines appears to use this parameter to determine the size of the
    // internal buffer to use.
    PicamError error = Picam_SetParameterLargeIntegerValue(internal->model_handle, PicamParameter_ReadoutCount, 0);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutCount.", error);

    // Create a buffer large enough to hold 5 frames. In normal operation,
    // only one should be required, but we include some overhead to be safe.
    size_t buffer_size = 5;
    piint frame_stride = 0;
    error = Picam_GetParameterIntegerValue(internal->model_handle, PicamParameter_ReadoutStride, &frame_stride);
    if (error != PicamError_None)
        print_error("Failed to set PicamParameter_ReadoutStride.", error);

    internal->image_buffer = (pibyte *)malloc(buffer_size*frame_stride*sizeof(pibyte));
    if (!internal->image_buffer)
        fatal_error(internal, "Failed to allocate frame buffer.");

    PicamAcquisitionBuffer buffer =
    {
        .memory = internal->image_buffer,
        .memory_size = buffer_size*frame_stride
    };

    error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
        fatal_error(internal, "Acquisition setup failed.");
    }

    // Commit parameter changes to hardware
    commit_camera_params(internal);

    // Register callback for acquisition status change / frame available
    callback_internal_ref = internal;
    error = PicamAdvanced_RegisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    if (error != PicamError_None)
    {
        print_error("PicamAdvanced_RegisterForAcquisitionUpdated failed.", error);
        fatal_error(internal, "Acquisition setup failed.");
    }

    return internal;
}

double camera_picam_update_camera_settings(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    PicamError error;

    // Validate and set readout port
    const PicamCollectionConstraint *port_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality,
                                               PicamConstraintCategory_Required, &port_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t port_id = pn_preference_char(CAMERA_READPORT_MODE);
    if (port_id >= port_constraint->values_count)
    {
        pn_log("Invalid port index: %d. Reset to %d.", port_id, 0);
        pn_preference_set_char(CAMERA_READPORT_MODE, 0);
        port_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcQuality,
                      (piint)(port_constraint->values_array[port_id]));
    Picam_DestroyCollectionConstraints(port_constraint);

    // Validate and set readout speed
    const PicamCollectionConstraint *speed_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed,
                                               PicamConstraintCategory_Required, &speed_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t speed_id = pn_preference_char(CAMERA_READSPEED_MODE);
    if (speed_id >= speed_constraint->values_count)
    {
        pn_log("Invalid speed index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        speed_id = 0;
    }
    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_AdcSpeed,
                                                            speed_constraint->values_array[speed_id]);
    if (error != PicamError_None)
        print_error("Failed to set Readout Speed.", error);
    Picam_DestroyCollectionConstraints(speed_constraint);

    // Validate and set readout gain
    const PicamCollectionConstraint *gain_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain,
                                               PicamConstraintCategory_Required, &gain_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcGain Constraints.");

    uint8_t gain_id = pn_preference_char(CAMERA_GAIN_MODE);
    if (speed_id >= gain_constraint->values_count)
    {
        pn_log("Invalid gain index: %d. Reset to %d.", speed_id, 0);
        pn_preference_set_char(CAMERA_READSPEED_MODE, 0);
        gain_id = 0;
    }
    set_integer_param(internal->model_handle, PicamParameter_AdcAnalogGain,
                      (piint)(gain_constraint->values_array[gain_id]));
    Picam_DestroyCollectionConstraints(gain_constraint);

    // Set temperature
    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_SensorTemperatureSetPoint,
                                                            pn_preference_int(CAMERA_TEMPERATURE)/100.0f);
    if (error != PicamError_None)
        print_error("Failed to set `PicamParameter_SensorTemperatureSetPoint'.", error);

    // Get chip dimensions
    const PicamRoisConstraint  *roi_constraint;
    if (Picam_GetParameterRoisConstraint(internal->model_handle, PicamParameter_Rois, PicamConstraintCategory_Required, &roi_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query ROIs Constraint.");

    // Get region definition
    const PicamRois *region;
    if (Picam_GetParameterRoisValue(internal->model_handle, PicamParameter_Rois, &region) != PicamError_None)
    {
        Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        fatal_error(internal, "Failed to query current ROI.");
    }

    // Set ROI to full chip, with requested binning
    unsigned char bin = pn_preference_char(CAMERA_BINNING);
    PicamRoi *roi = &region->roi_array[0];
    roi->x = roi_constraint->x_constraint.minimum;
    roi->y = roi_constraint->y_constraint.minimum;
    roi->width = roi_constraint->width_constraint.maximum;
    roi->height = roi_constraint->height_constraint.maximum;
    roi->x_binning = bin;
    roi->y_binning = bin;

    internal->frame_width  = (uint16_t)(roi_constraint->width_constraint.maximum) / bin;
    internal->frame_height = (uint16_t)(roi_constraint->height_constraint.maximum) / bin;

    if (Picam_SetParameterRoisValue(internal->model_handle, PicamParameter_Rois, region) != PicamError_None)
    {
    	Picam_DestroyRoisConstraints(roi_constraint);
        Picam_DestroyRois(region);
        fatal_error(internal, "Failed to set ROI");
    }

    Picam_DestroyRoisConstraints(roi_constraint);
    Picam_DestroyRois(region);

    // Query readout time
    piflt readout_time;
    error = Picam_GetParameterFloatingPointValue(internal->model_handle, PicamParameter_ReadoutTimeCalculation, &readout_time);
    if (error != PicamError_None)
        print_error("Failed to query readout time.", error);

    // convert from ms to s
    readout_time /= 1000;
    pn_log("Camera readout time is now %.2fs", readout_time);
    uint8_t exposure_time = pn_preference_char(EXPOSURE_TIME);
    if (exposure_time < readout_time)
    {
        unsigned char new_exposure = (unsigned char)(ceil(readout_time));
        pn_preference_set_char(EXPOSURE_TIME, new_exposure);
        pn_log("Increasing EXPOSURE_TIME to %d seconds.", new_exposure);
    }

    return readout_time;
}

uint8_t camera_picam_port_table(Camera *camera, void *_internal, struct camera_port_option **out_ports)
{
    struct internal *internal = _internal;

    const PicamCollectionConstraint *port_constraint;
    if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcQuality, PicamConstraintCategory_Required, &port_constraint) != PicamError_None)
        fatal_error(internal, "Failed to query AdcSpeed Constraints.");

    uint8_t port_count = port_constraint->values_count;
    struct camera_port_option *ports = calloc(port_count, sizeof(struct camera_port_option));
    if (!ports)
        fatal_error(internal, "Failed to allocate memory for %d readout ports.", port_count);

    const pichar *value;
    char str[100];
    for (uint8_t i = 0; i < port_count; i++)
    {
        struct camera_port_option *port = &ports[i];

        Picam_GetEnumerationString(PicamEnumeratedType_AdcQuality, port_constraint->values_array[i], &value);
        port->name = strdup(value);
        Picam_DestroyString(value);

        PicamError error = Picam_SetParameterIntegerValue(internal->model_handle, PicamParameter_AdcQuality, (piint)(port_constraint->values_array[i]));
        if (error != PicamError_None)
            fatal_error(internal, "Failed to set Readout Port.", error);

        const PicamCollectionConstraint *speed_constraint;
        if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcSpeed, PicamConstraintCategory_Required, &speed_constraint) != PicamError_None)
            fatal_error(internal, "Failed to query AdcSpeed Constraints.");

        port->speed_count = speed_constraint->values_count;
        port->speed = calloc(port->speed_count, sizeof(struct camera_speed_option));
        if (!port->speed)
            fatal_error(internal, "Failed to allocate memory for %d readout speeds.", port->speed_count);

        for (uint8_t j = 0; j < port->speed_count; j++)
        {
            struct camera_speed_option *speed = &port->speed[j];
            snprintf(str, 100, "%0.1f MHz", speed_constraint->values_array[j]);
            speed->name = strdup(str);

            PicamError error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_AdcSpeed, speed_constraint->values_array[j]);
            if (error != PicamError_None)
                fatal_error(internal, "Failed to set Readout Speed.", error);

            const PicamCollectionConstraint *gain_constraint;
            if (Picam_GetParameterCollectionConstraint(internal->model_handle, PicamParameter_AdcAnalogGain, PicamConstraintCategory_Required, &gain_constraint) != PicamError_None)
                fatal_error(internal, "Failed to query AdcGain Constraints.");

            speed->gain_count = gain_constraint->values_count;
            speed->gain = calloc(speed->gain_count, sizeof(struct camera_gain_option));
            if (!speed->gain)
                fatal_error(internal, "Failed to allocate memory for readout gains.");

            for (uint8_t k = 0; k < speed->gain_count; k++)
            {
                Picam_GetEnumerationString(PicamEnumeratedType_AdcAnalogGain, gain_constraint->values_array[k], &value);
                speed->gain[k].name = strdup(value);
                Picam_DestroyString(value);
            }
            Picam_DestroyCollectionConstraints(gain_constraint);
        }
        Picam_DestroyCollectionConstraints(speed_constraint);
    }
    Picam_DestroyCollectionConstraints(port_constraint);

    *out_ports = ports;
    return port_count;
}

void camera_picam_uninitialize(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    // Shutdown camera and PICAM
    PicamAcquisitionBuffer buffer = {.memory = NULL, .memory_size = 0};
    PicamError error = PicamAdvanced_SetAcquisitionBuffer(internal->device_handle, &buffer);
    if (error != PicamError_None)
        print_error("PicamAdvanced_SetAcquisitionBuffer failed.", error);
    free(internal->image_buffer);

    PicamAdvanced_UnregisterForAcquisitionUpdated(internal->device_handle, acquisitionUpdatedCallback);
    PicamAdvanced_CloseCameraDevice(internal->device_handle);
    Picam_UninitializeLibrary();
}

void camera_picam_start_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    PicamError error;

    // The ProEM camera cannot be operated in trigger = download mode
    // Instead, we set an exposure period 20ms shorter than the trigger period,
    // giving the camera a short window to be responsive for the next trigger
    // TODO: Investigate the optimum period difference to use
    piflt exptime = 1000*pn_preference_char(EXPOSURE_TIME) - 20;
    error = Picam_SetParameterFloatingPointValue(internal->model_handle, PicamParameter_ExposureTime, exptime);
    if (error != PicamError_None)
        print_error("PicamParameter_ExposureTime failed.", error);

    // Keep the shutter open during the sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysOpen);

    commit_camera_params(internal);

    error = Picam_StartAcquisition(internal->model_handle);
    if (error != PicamError_None)
    {
        print_error("Picam_StartAcquisition failed.", error);
        fatal_error(internal, "Aquisition initialization failed.");
    }
}

// Stop an acquisition sequence
void camera_picam_stop_acquiring(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;

    pibln running;
    PicamError error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
    {
        error = Picam_StopAcquisition(internal->model_handle);
        if (error != PicamError_None)
            print_error("Picam_StopAcquisition failed.", error);
    }

    error = Picam_IsAcquisitionRunning(internal->model_handle, &running);
    if (error != PicamError_None)
        print_error("Picam_IsAcquisitionRunning failed.", error);

    if (running)
        fatal_error(internal, "Failed to stop acquisition");

    // Close the shutter until the next exposure sequence
    set_integer_param(internal->model_handle, PicamParameter_ShutterTimingMode, PicamShutterTimingMode_AlwaysClosed);
    commit_camera_params(internal);
}

// New frames are notified by callback, so we don't need to do anything here
void camera_picam_tick(void *_args) {}

double camera_picam_read_temperature(Camera *camera, void *_internal)
{
    struct internal *internal = _internal;
    return read_temperature(internal->model_handle);
}

