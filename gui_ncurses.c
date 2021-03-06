/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
    #include <ncurses/ncurses.h>
    #include <ncurses/panel.h>
#else
    #include <ncurses.h>
    #include <panel.h>
#endif

#include "gui.h"
#include "timer.h"
#include "camera.h"
#include "preferences.h"
#include "platform.h"
#include "main.h"

// Input parsing modes
typedef enum
{
    INPUT_MAIN,
    INPUT_EXPOSURE,
    INPUT_PARAMETERS,
    INPUT_RUN_PREFIX,
    INPUT_FRAME_DIR,
    INPUT_FRAME_NUMBER,
    INPUT_OBJECT_NAME,
    INPUT_FRAME_TYPE,
    INPUT_COUNTDOWN_NUMBER
} PNUIInputType;

extern TimerUnit *timer;
extern Camera *camera;

WINDOW  *time_window, *camera_window, *acquisition_window,
        *command_window, *metadata_window, *log_window,
        *status_window, *separator_window,
        *input_window, *parameters_window, *frametype_window;

PANEL   *time_panel, *camera_panel, *acquisition_panel,
        *command_panel, *metadata_panel, *log_panel,
        *status_panel, *separator_panel,
        *input_panel, *parameters_panel, *frametype_panel;

PNCameraMode last_camera_mode;
double last_camera_temperature;
int last_calibration_framecount;
int last_run_number;
int last_camera_downloading;
uint16_t last_exposure_time;
PNUIInputType input_type = INPUT_MAIN;

// A circular buffer for storing log messages
static char *log_messages[256];
static uint8_t log_position;
static uint8_t last_log_position;
static bool should_quit = false;

static WINDOW *create_time_window()
{
    int x = 0;
    int y = 0;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Timer Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "   Status:");
    mvwaddstr(win, 2, 2, "  PC Time:");
    mvwaddstr(win, 3, 2, " UTC Time:");
    mvwaddstr(win, 4, 2, "Countdown:");
    
    return win;
}

static void update_time_window()
{
    // PC time
    char strtime[30];
    time_t t = time(NULL);
    strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
    mvwaddstr(time_window, 2, 13, strtime);

    // GPS time
    if (timer_gps_status(m_timerRef) == GPS_ACTIVE)
    {
        TimerTimestamp ts = timer_current_timestamp(m_timerRef);
        mvwaddstr(time_window, 1, 13, (ts.locked ? "Locked     " : "Unlocked   "));
        mvwprintw(time_window, 3, 13, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);

        if (ts.exposure_progress != last_exposure_time)
            mvwprintw(time_window, 4, 13, "%03d        ", ts.exposure_progress);
        else
            mvwaddstr(time_window, 4, 13, "Disabled    ");
    }
    else
    {
        mvwaddstr(time_window, 1, 13, "Unavailable");
        mvwaddstr(time_window, 3, 13, "Unavailable");
        mvwaddstr(time_window, 4, 13, "Unavailable");
    }
}

static WINDOW *create_camera_window()
{
    int x = 0;
    int y = 6;
    int w = 34;
    int h = 4;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Camera Information ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "     Status:");
    mvwaddstr(win, 2, 2, "Temperature:");
    
    return win;
}

static void update_camera_window(PNCameraMode mode, int camera_downloading, double temp)
{
    // Camera status
    char *label;
    switch(mode)
    {
        default:
        case INITIALISING:
        case ACQUIRE_START:
        case ACQUIRE_STOP:
            label = "Initialising";
        break;
        case IDLE:
            label = "Idle        ";
        break;
        case ACQUIRING:
            if (camera_downloading)
                label = "Downloading ";
            else
                label = "Acquiring   ";
        break;
        case SHUTDOWN:
            label = "Closing     ";
        break;
    }
    mvwaddstr(camera_window, 1, 15, label);

    // Camera temperature
    char *tempstring = "Unavailable";
    char tempbuf[10];
    
    if (mode == ACQUIRING || mode == IDLE)
    {
        snprintf(tempbuf, 10, "%0.02f C     ", temp);
        tempstring = tempbuf;
    }
    mvwaddstr(camera_window, 2, 15, tempstring);
}

static WINDOW *create_acquisition_window()
{
    int x = 0;
    int y = 16;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Acquisition ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, " Exposure:");
    mvwaddstr(win, 2, 2, "     Type:");
    mvwaddstr(win, 3, 2, "    Burst:");
    mvwaddstr(win, 4, 2, " Filename:");
    
    return win;
}

static void update_acquisition_window()
{
    PNFrameType type = pn_preference_char(OBJECT_TYPE);
    uint16_t exptime = pn_preference_int(EXPOSURE_TIME);
    int burst_countdown = pn_preference_int(BURST_COUNTDOWN);
    char *run_prefix = pn_preference_string(RUN_PREFIX);
    int run_number = pn_preference_int(RUN_NUMBER);

    char *typename;
    switch (type)
    {
        case OBJECT_DARK:
            typename = "Dark  ";
            break;
        case OBJECT_FLAT:
            typename = "Flat  ";
            break;
        case OBJECT_FOCUS:
            typename = "Focus ";
            break;
        case OBJECT_TARGET:
            typename = "Target";
            break;
    }

    mvwprintw(acquisition_window, 1, 13, "%d seconds   ", exptime);
    mvwaddstr(acquisition_window, 2, 13, typename);

    if (pn_prefererence_char(BURST_ENABLED))
        mvwprintw(acquisition_window, 3, 13, "%d   ", burst_countdown);
    else
        mvwaddstr(acquisition_window, 3, 13, "N/A        ");

    mvwaddstr(acquisition_window, 4, 13, "                    ");
    mvwprintw(acquisition_window, 4, 13, "%s-%04d.fits.gz", run_prefix, run_number);
    free(run_prefix);
}

static WINDOW *create_metadata_window()
{
    int x = 0;
    int y = 10;
    int w = 34;
    int h = 6;
    
    WINDOW *win = newwin(h, w, y, x);
    box(win, 0, 0);
    
    char *title = " Metadata ";
    mvwaddstr(win, 0, (w-strlen(title))/2, title);
    mvwaddstr(win, 1, 2, "Observatory:");
    mvwaddstr(win, 2, 2, "  Observers:");
    mvwaddstr(win, 3, 2, "  Telescope:");
    mvwaddstr(win, 4, 2, "     Target:");

    return win;
}

static void update_metadata_window()
{
    char *observatory = pn_preference_string(OBSERVATORY);
    mvwaddstr(metadata_window, 1, 15, observatory);
    free(observatory);

    char *observers = pn_preference_string(OBSERVERS);
    mvwaddstr(metadata_window, 2, 15, observers);
    free(observers);

    char *telescope = pn_preference_string(TELESCOPE);
    mvwaddstr(metadata_window, 3, 15, telescope);
    free(telescope);

    char *object = pn_preference_string(OBJECT_NAME);
    mvwaddstr(metadata_window, 4, 15, object);
    free(object);
}

static WINDOW *create_log_window()
{
    int row,col;
    getmaxyx(stdscr,row,col);

    int x = 35;
    int y = 0;
    int w = col - 34;
    int h = row - 4;
    return newwin(h, w, y, x);
}

static void update_log_window()
{
    int height, width;
    getmaxyx(log_window, height, width);

    // First erase the screen
    wclear(log_window);

    // Redraw messages
    for (int i = 0; i < height && i < 256; i++)
    {
        unsigned char j = (unsigned char)(log_position - height + i + 1);
        if (log_messages[j] != NULL)
            mvwaddstr(log_window, i, 0, log_messages[j]);
    }
}

void pn_ui_log_line(char *msg)
{
    log_position++;
    if (log_messages[log_position] != NULL)
        free(log_messages[log_position]);

    log_messages[log_position] = strdup(msg);
}


static WINDOW *create_status_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 35;
    int y = row-3;
    int w = col-35;
    int h = 1;

    WINDOW *win = newwin(h, w, y, x);
    mvwaddstr(win, 0, 5, "Acquiring:");
    mvwaddstr(win, 0, 27, "Saving:");
    return win;
}

static void update_status_window(PNCameraMode mode)
{
    unsigned char save = pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save();
    wattron(status_window, A_STANDOUT);
    mvwaddstr(status_window, 0, 16, (mode == ACQUIRING ? " YES " : mode == IDLE ? " NO " : " ... "));
    wattroff(status_window, A_STANDOUT);
    waddstr(status_window, " ");
    wattron(status_window, A_STANDOUT);
    mvwaddstr(status_window, 0, 35, (save ? " YES " : " NO "));
    wattroff(status_window, A_STANDOUT);
    waddstr(status_window, " ");
}

static WINDOW *create_separator_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);
    WINDOW *win = newwin(1, col, row-2, 0);
    mvwhline(win, 0, 0, 0, col);
    return win;
}

static WINDOW *create_command_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    int x = 0;
    int y = row-1;
    int w = col;
    int h = 1;
    return newwin(h, w, y, x);
}

static void print_command_option(WINDOW *w, int indent, char *hotkey, char *format, ...)
{
    if (indent)
        waddstr(w, "  ");

    wattron(w, A_STANDOUT);
    waddstr(w, hotkey);
    wattroff(w, A_STANDOUT);
    waddstr(w, " ");

    va_list args;
    va_start(args, format);
    vwprintw(w, format, args);
    va_end(args);
}

static void update_command_window(PNCameraMode mode)
{
    int row,col;
    getmaxyx(stdscr,row,col);
    wclear(command_window);

    if (should_quit)
        return;

    unsigned char save = pn_preference_char(SAVE_FRAMES);
    unsigned char burst_enabled = pn_preference_char(BURST_ENABLED);
    int burst_countdown = pn_preference_int(BURST_COUNTDOWN);

    // Acquire toggle
    if (mode == IDLE || mode == ACQUIRING)
        print_command_option(command_window, false, "^A", "Acquire", mode == ACQUIRING ? "Stop " : "");

    // Save toggle
    if (!burst_enabled || burst_countdown > 0)
        print_command_option(command_window, true, "^S", "%s Saving", save ? "Stop" : "Start");

    // Display parameter panel
    if (!save || !pn_preference_allow_save())
        print_command_option(command_window, true, "^P", "Edit Parameters");

    // Display exposure panel
    if (mode == IDLE)
        print_command_option(command_window, true, "^E", "Set Exposure");

    print_command_option(command_window, true, "^C", "Quit");
}

static WINDOW *create_parameters_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    return newwin(1, col, row-1, 0);
}

static WINDOW *create_error_window(char *msg)
{
    int row, col;
    getmaxyx(stdscr, row, col);

    WINDOW *win = newwin(row, col, 0, 0);
    box(win, 0, 0);
    char *title = " FATAL ERROR ";
    mvwaddstr(win, 0, (col-strlen(title))/2, title);
    mvwaddstr(win, (row-1)/2, (col-strlen(msg))/2, msg);

    return win;
}

static void update_parameters_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);
    wclear(parameters_window);
    print_command_option(parameters_window, false, "^T", "Type");

    if (pn_preference_char(OBJECT_TYPE) == OBJECT_TARGET)
        print_command_option(parameters_window, true, "^O", "Target Name");
    else
        print_command_option(parameters_window, true, "^D", "Set Countdown");

    print_command_option(parameters_window, true, "^S", "Frame Dir");
    print_command_option(parameters_window, true, "^P", "Run Prefix");
    print_command_option(parameters_window, true, "^N", "Frame #");
    print_command_option(parameters_window, true, "RET", "Back");
}


static WINDOW *create_frametype_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    WINDOW *ret = newwin(1, col, row-1, 0);
    print_command_option(ret, false, " D ", "Dark");
    print_command_option(ret, true, " F ", "Flat");
    print_command_option(ret, true, " C ", "Focus");
    print_command_option(ret, true, " T ", "Target");
    return ret;
}

int input_entry_margin = 0;
const size_t input_entry_buf_len = 1024;
char input_entry_buf[1024];
int input_entry_length = 0;

static WINDOW *create_input_window()
{
    int row, col;
    getmaxyx(stdscr, row, col);

    WINDOW *win = newwin(1, col, row-1, 0);
    return win;
}

static void set_input_window_msg(char *msg)
{
    input_entry_margin = strlen(msg) + 1;
    mvwaddstr(input_window, 0, 1, msg);
}

static void update_input_window()
{
    mvwaddnstr(input_window, 0, input_entry_margin, input_entry_buf, input_entry_length);
    wclrtoeol(input_window);
}

void pn_ui_new(Camera *camera, TimerUnit *timer)
{
    // Initialize circular buffer for log display
    last_log_position = log_position = 0;
    for (int i = 0; i < 256; i++)
        log_messages[i] = NULL;

    initscr();
    noecho();
    raw();

    // Create windows
    time_window = create_time_window();
    camera_window = create_camera_window();
    acquisition_window = create_acquisition_window();
    command_window = create_command_window();
    metadata_window = create_metadata_window();
    log_window = create_log_window();
    status_window = create_status_window();

    separator_window = create_separator_window();
    input_window = create_input_window();
    parameters_window = create_parameters_window();
    frametype_window = create_frametype_window();

    // Create panels
    time_panel = new_panel(time_window);
    camera_panel = new_panel(camera_window);
    acquisition_panel = new_panel(acquisition_window);
    metadata_panel = new_panel(metadata_window);
    log_panel = new_panel(log_window);
    status_panel = new_panel(status_window);
    command_panel = new_panel(command_window);

    separator_panel = new_panel(separator_window);
    input_panel = new_panel(input_window);
    parameters_panel = new_panel(parameters_window);
    frametype_panel = new_panel(frametype_window);

    // Set initial state
    last_camera_mode = camera_mode(camera);
    last_camera_temperature = camera_temperature(camera);

    update_log_window();
    update_status_window(last_camera_mode);
    update_command_window(last_camera_mode);
    update_metadata_window();

    last_camera_downloading = timer_mode(timer) == TIMER_READOUT;

    update_acquisition_window();
    update_time_window();
    update_camera_window(last_camera_mode, last_camera_downloading, last_camera_temperature);
    hide_panel(input_panel);
    hide_panel(parameters_panel);
    hide_panel(frametype_panel);

    // Only wait for 100ms for input so we can keep the ui up to date
    // *and* respond timely to input
    timeout(100);
}

void pn_ui_show_fatal_error()
{
    WINDOW *error_window = create_error_window("See log for more details");
    PANEL *error_panel = new_panel(error_window);
    int row,col;
    getmaxyx(stdscr, row, col);
    timeout(250);

    // Blink the screen annoyingly fast to grab attention until a key is pressed
    int ch = ERR;
    while (ch == ERR)
    {
        flash();
        update_panels();
        doupdate();
        move(row-1, col-1);
        ch = getch();
    }
    del_panel(error_panel);
    delwin(error_window);
    should_quit = true;
}

bool pn_ui_update()
{
    int ch = ERR;

    // Read once at the start of the loop so values remain consistent
    PNCameraMode mode = camera_mode(camera);
    double temperature = camera_temperature(camera);

    bool camera_downloading = timer_mode(timer) == TIMER_READOUT;
    unsigned char is_input = false;
    while (!should_quit && (ch = getch()) != ERR)
    {
        // Resized terminal window
        if (ch == 0x19a)
        {
            WINDOW *temp_win = time_window;
            time_window = create_time_window();
            replace_panel(time_panel, time_window);
            delwin(temp_win);
            update_time_window();

            temp_win = camera_window;
            camera_window = create_camera_window();
            replace_panel(camera_panel, camera_window);
            delwin(temp_win);
            update_camera_window(mode, camera_downloading, temperature);

            temp_win = acquisition_window;
            acquisition_window = create_acquisition_window();
            replace_panel(acquisition_panel, acquisition_window);
            delwin(temp_win);
            update_acquisition_window();

            temp_win = command_window;
            command_window = create_command_window();
            replace_panel(command_panel, command_window);
            delwin(temp_win);
            update_command_window(mode);

            temp_win = metadata_window;
            metadata_window = create_metadata_window();
            replace_panel(metadata_panel, metadata_window);
            delwin(temp_win);
            update_metadata_window();

            temp_win = log_window;
            log_window = create_log_window();
            replace_panel(log_panel, log_window);
            delwin(temp_win);
            update_log_window();

            temp_win = status_window;
            status_window = create_status_window();
            replace_panel(status_panel, status_window);
            delwin(temp_win);
            update_status_window(mode);

            temp_win = separator_window;
            separator_window = create_separator_window();
            replace_panel(separator_panel, separator_window);
            delwin(temp_win);

            int row, col;
            getmaxyx(stdscr, row, col);
            move_panel(command_panel, row - 1, 0);
            move_panel(input_panel, row - 1, 0);
            move_panel(parameters_panel, row - 1, 0);
            move_panel(frametype_panel, row - 1, 0);
            continue;
        }

        switch (input_type)
        {
            case INPUT_MAIN:
                switch (ch)
                {
                    case 0x01: // ^A - Toggle Acquire
                        if (mode == IDLE)
                        {
                            clear_queued_data(true);
                            camera_start_exposure(camera, !pn_preference_char(CAMERA_DISABLE_SHUTTER));
                            bool use_monitor = !camera_is_simulated(camera) && pn_preference_char(TIMER_MONITOR_LOGIC_OUT);
                            timer_start_exposure(timer, pn_preference_int(EXPOSURE_TIME), use_monitor);
                        }
                        else if (mode == ACQUIRING)
                        {
                            camera_stop_exposure(camera);
                            timer_stop_exposure(timer);
                        }
                        break;
                    case 0x05: // ^E - Set Exposure
                        if (mode != IDLE)
                            break;

                        input_type = INPUT_EXPOSURE;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", pn_preference_char(EXPOSURE_TIME));
                        set_input_window_msg("Enter an exposure time: ");
                        update_input_window();

                        hide_panel(command_panel);
                        show_panel(input_panel);
                    break;
                    case 0x10: // ^P - Edit Parameters
                        if (pn_preference_char(SAVE_FRAMES) && pn_preference_allow_save())
                            break;

                        input_type = INPUT_PARAMETERS;
                        hide_panel(command_panel);
                        update_parameters_window();
                        show_panel(parameters_panel);
                    break;
                    case 0x13: // ^S - Toggle Save
                        // Can't enable saving for calibration frames after the target count has been reached
                        if (!pn_preference_allow_save())
                        {
                            pn_log("Unable to toggle save: countdown is zero.");
                            break;
                        }

                        unsigned char save = pn_preference_toggle_save();
                        update_status_window(mode);
                        update_command_window(mode);
                        pn_log("%s saving.", save ? "Enabled" : "Disabled");
                    break;
                    case 0x03: // ^C - Quit
                        should_quit = true;
                        pn_log("Shutting down...");
                        update_command_window(mode);
                    break;
                }
            break;
            case INPUT_PARAMETERS:
                switch (ch)
                {
                    case '\n': // Back
                        input_type = INPUT_MAIN;
                        hide_panel(parameters_panel);
                        show_panel(command_panel);
                        break;
                    case 0x10: // ^P - Run Prefix
                        input_type = INPUT_RUN_PREFIX;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%s", pn_preference_string(RUN_PREFIX));
                        set_input_window_msg("Run Prefix: ");
                        is_input = true;
                        break;
                    case 0x13: // ^S - Frame dir
                        input_type = INPUT_FRAME_DIR;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%s", pn_preference_string(OUTPUT_DIR));
                        set_input_window_msg("Output path: ");
                        is_input = true;
                        break;
                    case 0x0f: // ^O - Object name
                        input_type = INPUT_OBJECT_NAME;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%s", pn_preference_string(OBJECT_NAME));
                        set_input_window_msg("Target Name: ");
                        is_input = true;
                        break;
                    case 0x0e: // ^N - Frame #
                        input_type = INPUT_FRAME_NUMBER;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", pn_preference_int(RUN_NUMBER));
                        set_input_window_msg("Frame #: ");
                        is_input = true;
                        break;
                    case 0x14: // ^T - Frame Type
                        input_type = INPUT_FRAME_TYPE;
                        hide_panel(parameters_panel);
                        show_panel(frametype_panel);
                        break;
                    case 0x04: // ^D - Countdown
                        input_type = INPUT_COUNTDOWN_NUMBER;

                        input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", pn_preference_int(BURST_COUNTDOWN));
                        set_input_window_msg("Countdown #: ");
                        is_input = true;
                        break;
                }

                if (is_input && ch != '\n')
                {
                    update_input_window();
                    hide_panel(parameters_panel);
                    show_panel(input_panel);
                }
                break;
            case INPUT_EXPOSURE:
            case INPUT_FRAME_NUMBER:
            case INPUT_COUNTDOWN_NUMBER:
                if (ch == '\n')
                {
                    input_entry_buf[input_entry_length] = '\0';
                    int new = atoi(input_entry_buf);

                    if (input_type == INPUT_EXPOSURE)
                    {
                        uint16_t oldexp = pn_preference_int(EXPOSURE_TIME);
                        if (new > 65535)
                        {
                            // Invalid entry
                            pn_log("Maximum exposure: 65535 seconds.");
                            input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", oldexp);
                        }
                        else
                        {
                            input_type = INPUT_MAIN;
                            hide_panel(input_panel);
                            show_panel(command_panel);

                            if (oldexp != new)
                            {
                                // Update preferences
                                pn_preference_set_int(EXPOSURE_TIME, new);
                                update_acquisition_window();
                                pn_log("Exposure set to %d seconds.", new);
                            }
                        }
                    }
                    else if (input_type == INPUT_FRAME_NUMBER)
                    {
                        unsigned char oldframe = pn_preference_int(RUN_NUMBER);
                        if (new < 0)
                        {
                            // Invalid entry
                            input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", oldframe);
                        }
                        else
                        {
                            input_type = INPUT_PARAMETERS;
                            hide_panel(input_panel);
                            show_panel(parameters_panel);

                            if (oldframe != new)
                            {
                                // Update preferences
                                pn_preference_set_int(RUN_NUMBER, new);
                                update_acquisition_window();
                                pn_log("Frame # set to %d.", new);
                            }
                        }
                    }
                    else if (input_type == INPUT_COUNTDOWN_NUMBER)
                    {
                        unsigned char oldcount = pn_preference_int(BURST_COUNTDOWN);
                        if (new < 0)
                        {
                            // Invalid entry
                            input_entry_length = snprintf(input_entry_buf, input_entry_buf_len, "%d", oldcount);
                        }
                        else
                        {
                            input_type = INPUT_PARAMETERS;
                            hide_panel(input_panel);
                            show_panel(parameters_panel);

                            if (oldcount != new)
                            {
                                // Update preferences
                                pn_preference_set_int(BURST_COUNTDOWN, new);
                                update_acquisition_window();
                                update_status_window(mode);
                                update_command_window(mode);
                                pn_log("Countdown # set to %d.", new);
                            }
                        }
                    }
                }
                else if ((ch == 0x7f || ch == 0x08) && input_entry_length > 0) // Backspace
                    --input_entry_length;
                else if (isdigit(ch) && input_entry_length < 1024 - 1)
                    input_entry_buf[input_entry_length++] = ch;

                update_input_window();
            break;
            case INPUT_FRAME_DIR:
            case INPUT_RUN_PREFIX:
            case INPUT_OBJECT_NAME:
                if (ch == '\n')
                {
                    input_entry_buf[input_entry_length] = '\0';

                    if (input_type == INPUT_RUN_PREFIX)
                    {
                        char *oldprefix = pn_preference_string(RUN_PREFIX);

                        if (strcmp(oldprefix, input_entry_buf))
                        {
                            // Update preferences
                            pn_preference_set_string(RUN_PREFIX, input_entry_buf);
                            update_acquisition_window();
                            pn_log("Run prefix set to `%s'.", input_entry_buf);
                        }
                    }
                    else if (input_type == INPUT_FRAME_DIR)
                    {
                        char *olddir = pn_preference_string(OUTPUT_DIR);
                        char *path = canonicalize_path(input_entry_buf);

                        if (strcmp(olddir, path))
                        {
                            // Update preferences
                            pn_preference_set_string(OUTPUT_DIR, path);
                            update_acquisition_window();
                            pn_log("Frame dir set to `%s'.", path);
                        }
                        free(path);
                    }
                    else if (input_type == INPUT_OBJECT_NAME)
                    {
                        char *oldname = pn_preference_string(OBJECT_NAME);
                        if (strcmp(oldname, input_entry_buf))
                        {
                            // Update preferences
                            pn_preference_set_string(OBJECT_NAME, input_entry_buf);
                            update_metadata_window();
                            pn_log("Object name set to `%s'.", input_entry_buf);
                        }
                    }
                    input_type = INPUT_PARAMETERS;
                    hide_panel(input_panel);
                    show_panel(parameters_panel);
                }
                else if (ch == 0x7f || ch == 0x08) // Backspace
                {
                    if (input_entry_length > 0)
                        --input_entry_length;
                }
                else if (isascii(ch) && input_entry_length < 1024 - 1)
                    input_entry_buf[input_entry_length++] = ch;
                
                update_input_window();
            break;
            case INPUT_FRAME_TYPE:
                if (!isalpha(ch))
                    break;

                unsigned char type = 0xFF;
                char *type_name = "INVALID";
                switch (tolower(ch))
                {
                    case 'd':
                        type = OBJECT_DARK;
                        type_name = "Dark";
                    break;
                    case 'f':
                        type = OBJECT_FLAT;
                        type_name = "Flat";
                    break;
                    case 'c':
                        type = OBJECT_FOCUS;
                        type_name = "Focus";
                    break;
                    case 't':
                        type = OBJECT_TARGET;
                        type_name = "Target";
                    break;
                }
                if (type == 0xFF)
                    break;

                unsigned char oldtype = pn_preference_char(OBJECT_TYPE);
                if (type != oldtype)
                {
                    pn_preference_set_char(OBJECT_TYPE, type);
                    update_acquisition_window();
                    update_parameters_window();
                    update_metadata_window();
                    pn_log("Frame type set to `%s'.", type_name);

                    input_type = INPUT_PARAMETERS;
                    hide_panel(input_panel);
                    show_panel(parameters_panel);
                }
            break;
        }
    }
    update_time_window();

    if (log_position != last_log_position)
    {
        update_log_window();
        last_log_position = log_position;
    }

    if (last_camera_mode != mode ||
        last_camera_downloading != camera_downloading)
    {
        update_command_window(mode);
        update_status_window(mode);
        update_camera_window(mode, camera_downloading, temperature);
        last_camera_mode = mode;
        last_camera_downloading = camera_downloading;
    }

    if (last_camera_temperature != temperature)
    {
        update_camera_window(mode, camera_downloading, temperature);
        last_camera_temperature = temperature;
    }

    int burst_countdown = pn_preference_int(BURST_COUNTDOWN);
    int run_number = pn_preference_int(RUN_NUMBER);
    uint16_t exposure_time = pn_preference_int(EXPOSURE_TIME);

    if (remaining_frames != last_calibration_framecount ||
        run_number != last_run_number ||
        exposure_time != last_exposure_time)
    {
        update_acquisition_window();
        update_status_window(mode);
        last_burst_countdown = burst_countdown;
        last_run_number = run_number;
        last_exposure_time = exposure_time;
    }

    update_panels();
    doupdate();

    return should_quit;
}

void pn_ui_free()
{
    // Destroy ui
    del_panel(time_panel);
    del_panel(camera_panel);
    del_panel(acquisition_panel);
    del_panel(metadata_panel);
    del_panel(log_panel);
    del_panel(command_panel);
    del_panel(input_panel);
    del_panel(parameters_panel);
    del_panel(frametype_panel);

    delwin(time_window);
    delwin(camera_window);
    delwin(acquisition_window);
    delwin(metadata_window);
    delwin(log_window);
    delwin(command_window);
    delwin(input_window);
    delwin(parameters_window);
    delwin(frametype_window);

    for (int i = 0; i < 256; i++)
        free(log_messages[i]);

    endwin();
}
