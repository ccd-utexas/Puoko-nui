/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef GUI_FLTK_H
#define GUI_FLTK_H

// FLTK headers are full of (void *)0 instead of NULLs
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#pragma GCC diagnostic ignored "-Wlong-long"
#include <FL/Fl.H>
#pragma GCC diagnostic warning "-Wlong-long"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Multi_Browser.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>

#pragma GCC diagnostic warning "-Wint-to-pointer-cast"

extern "C" {
    #include "timer.h"
    #include "camera.h"
    #include "preferences.h"
    #include "main.h"
    #include "platform.h"
    #include "gui.h"
}

class FLTKGui
{
public:
	FLTKGui(Camera *camera, TimerUnit *timer);
	~FLTKGui();
    void addLogLine(const char *msg);
    bool update();
    void updateTimerGroup();
    void updateCameraGroup();
    void updateAcquisitionGroup();
    void updateButtonGroup();

private:
    static Fl_Group *createGroupBox(int y, int h, const char *label);
    static Fl_Output *createOutputLabel(int y, const char *label);
    void createTimerGroup();
    void createCameraGroup();
    void createAcquisitionGroup();
    void createLogGroup();
    void createButtonGroup();

    void cameraRebuildPortTree(uint8_t port_id, uint8_t speed_id, uint8_t gain_id);
    void createCameraWindow();
    void showCameraWindow();
    void createMetadataWindow();
    void showMetadataWindow();

    static void cameraPortSpeedGainChangedCallback(Fl_Widget *input, void *userdata);
    static void buttonMetadataPressed(Fl_Widget* o, void *v);
    static void buttonCameraPressed(Fl_Widget* o, void *v);
    static void buttonAcquirePressed(Fl_Widget* o, void *v);
    static void buttonSavePressed(Fl_Widget* o, void *v);
    static void buttonQuitPressed(Fl_Widget* o, void *v);

    static void buttonCameraConfirmPressed(Fl_Widget* o, void *v);
    static void buttonMetadataConfirmPressed(Fl_Widget* o, void *v);

    static void metadataFrameTypeChangedCallback(Fl_Widget *input, void *v);

    static void closeMainWindowCallback(Fl_Widget *window, void *v);

    Camera *m_cameraRef;
    TimerUnit *m_timerRef;

    Fl_Double_Window *m_mainWindow;

    // Timer info
    Fl_Group *m_timerGroup;
    Fl_Output *m_timerPCTimeOutput;
    Fl_Output *m_timerUTCTimeOutput;
    Fl_Output *m_timerUTCDateOutput;
    Fl_Output *m_timerExposureOutput;

    // Camera info
    Fl_Group *m_cameraGroup;
    Fl_Output *m_cameraStatusOutput;
    Fl_Output *m_cameraTemperatureOutput;
    Fl_Output *m_cameraReadoutOutput;

    // Acquisition info
    Fl_Group *m_acquisitionGroup;
    Fl_Output *m_acquisitionObserversOutput;
    Fl_Output *m_acquisitionTypeOutput;
    Fl_Output *m_acquisitionTargetOutput;
    Fl_Output *m_acquisitionRemainingOutput;
    Fl_Output *m_acquisitionFilenameOutput;
    
    // Log panel
    Fl_Multi_Browser *m_logDisplay;
    size_t m_logEntries;

    // Action buttons
    Fl_Button *m_buttonMetadata;
    Fl_Button *m_buttonCamera;
    Fl_Toggle_Button *m_buttonAcquire;
    Fl_Toggle_Button *m_buttonSave;
    Fl_Button *m_buttonQuit;

    // Temporary state comparables
    PNCameraMode cached_camera_mode;
    double cached_camera_temperature;
    double cached_camera_readout;
    int cached_calibration_framecount;
    int cached_run_number;
    uint8_t cached_exposure_time;
    TimerMode cached_timer_mode;
    bool cached_subsecond_mode;

    // Camera window
    Fl_Double_Window *m_cameraWindow;
    Fl_Choice *m_cameraPortInput;
    Fl_Choice *m_cameraSpeedInput;
    Fl_Choice *m_cameraGainInput;

    Fl_Spinner *m_cameraExposureSpinner;
    Fl_Float_Input *m_cameraTemperatureInput;
    Fl_Spinner *m_cameraBinningSpinner;
    Fl_Button *m_cameraButtonConfirm;

    Fl_Spinner *m_cameraWindowX;
    Fl_Spinner *m_cameraWindowY;
    Fl_Spinner *m_cameraWindowWidth;
    Fl_Spinner *m_cameraWindowHeight;

    // Metadata window
    Fl_Double_Window *m_metadataWindow;
    Fl_Button *m_metadataButtonConfirm;

    Fl_Input *m_metadataOutputDir;
    Fl_Input *m_metadataRunPrefix;
    Fl_Int_Input *m_metadataRunNumber;

    Fl_Choice *m_metadataFrameTypeInput;
    Fl_Input *m_metadataTargetInput;
    Fl_Int_Input *m_metadataCountdownInput;
    Fl_Input *m_metadataObserversInput;
    Fl_Input *m_metadataObservatoryInput;
    Fl_Input *m_metadataTelecopeInput;
    Fl_Input *m_metadataFilterInput;
 };

#endif
