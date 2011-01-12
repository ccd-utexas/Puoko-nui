/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <time.h>
#include <pthread.h>
#include <fitsio.h>

#include <sys/select.h>
#include <xpa.h>
#include "view.h"

/* target type changed: hide or display the target name field */
static void targettype_changed(GtkWidget *widget, gpointer data)
{
	RangahauView *view = (RangahauView *)data;	
	gboolean visible = (gtk_combo_box_get_active(GTK_COMBO_BOX(view->target_combobox)) == OBJECT_TARGET);
	gtk_widget_set_visible(view->target_entry, visible);
}

/* Called when the "save" checkbox is changed */
void rangahau_set_output_editable(RangahauView *view, gboolean editable)
{
	/* Settings fields */
	gtk_editable_set_editable(GTK_EDITABLE(view->observers_entry), editable);
	gtk_widget_set_sensitive(view->observers_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->observatory_entry), editable);
	gtk_widget_set_sensitive(view->observatory_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->telescope_entry), editable);
	gtk_widget_set_sensitive(view->telescope_entry, editable);
	gtk_widget_set_sensitive(view->target_combobox, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->target_entry), editable);
	gtk_widget_set_sensitive(view->target_entry, editable);
	gtk_widget_set_sensitive(view->destination_btn, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->run_entry), editable);
	gtk_widget_set_sensitive(view->run_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->frame_entry), editable);
	gtk_widget_set_sensitive(view->frame_entry, editable);
}

static void save_changed(GtkWidget *widget, gpointer data)
{
	RangahauView *view = (RangahauView *)data;	
	gboolean set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view->save_checkbox));
	rangahau_set_output_editable(view, !set);
	if (set)
		rangahau_update_output_preferences(view);
}

void rangahau_update_camera_preferences(RangahauView *view)
{
	view->prefs->exposure_time = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view->exptime_entry));
	view->prefs->bin_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view->binsize_entry));
	rangahau_save_preferences(view->prefs, "preferences.dat");
}

void rangahau_update_output_preferences(RangahauView *view)
{
	rangahau_set_preference_string(view->prefs->output_directory,gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(view->destination_btn)));
	rangahau_set_preference_string(view->prefs->run_prefix, gtk_entry_get_text(GTK_ENTRY(view->run_entry)));
	view->prefs->run_number = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(view->frame_entry));

	view->prefs->object_type = gtk_combo_box_get_active(GTK_COMBO_BOX(view->target_combobox));
	rangahau_set_preference_string(view->prefs->object_name, gtk_entry_get_text(GTK_ENTRY(view->target_entry)));

	rangahau_set_preference_string(view->prefs->observers, gtk_entry_get_text(GTK_ENTRY(view->observers_entry)));
	rangahau_set_preference_string(view->prefs->observatory, gtk_entry_get_text(GTK_ENTRY(view->observatory_entry)));
	rangahau_set_preference_string(view->prefs->telescope, gtk_entry_get_text(GTK_ENTRY(view->telescope_entry)));
	rangahau_save_preferences(view->prefs, "preferences.dat");
}


/* Called when the startstop button is pressed to enable or disable the camera fields */
void rangahau_set_camera_editable(RangahauView *view, gboolean editable)
{
	gtk_editable_set_editable(GTK_EDITABLE(view->exptime_entry), editable);
	gtk_widget_set_sensitive(view->exptime_entry, editable);
	gtk_editable_set_editable(GTK_EDITABLE(view->binsize_entry), editable);
	gtk_widget_set_sensitive(view->binsize_entry, editable);
}

/* Return a GtkWidget containing the settings panel */
GtkWidget *rangahau_output_panel(RangahauView *view)
{
	/* Frame around the panel */
	GtkWidget *frame = gtk_frame_new("Output");

	/* Table */
	GtkWidget *table = gtk_table_new(5,3,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);
	GtkWidget *field;

	/* Observatory */
	field = gtk_label_new("Observatory:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	view->observatory_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->observatory_entry), view->prefs->observatory);
	gtk_entry_set_width_chars(GTK_ENTRY(view->observatory_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view->observatory_entry, 1,2,0,1);

	/* Telescope */
	field = gtk_label_new("Telescope:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,0,1);

	view->telescope_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->telescope_entry), view->prefs->telescope);
	gtk_table_attach_defaults(GTK_TABLE(table), view->telescope_entry, 3,4,0,1);

	/* Observers */
	field = gtk_label_new("Observers:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);
	view->observers_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->observers_entry), view->prefs->observers);
	gtk_entry_set_width_chars(GTK_ENTRY(view->observers_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view->observers_entry, 1,2,1,2);

	/* Observation type (dark, flat, target) */
	field = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,1,2);

	GtkWidget *object = gtk_hbox_new(FALSE, 0);
	view->target_combobox = gtk_combo_box_new_text();
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_DARK, "Dark");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_FLAT, "Flat");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(view->target_combobox), OBJECT_TARGET, "Target");
	gtk_combo_box_set_active(GTK_COMBO_BOX(view->target_combobox), view->prefs->object_type);
	g_signal_connect(view->target_combobox, "changed", G_CALLBACK (targettype_changed), view);
	gtk_box_pack_start(GTK_BOX(object), view->target_combobox, FALSE, FALSE, 0);

	view->target_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->target_entry), view->prefs->object_name);
	gtk_entry_set_width_chars(GTK_ENTRY(view->target_entry), 12);
	gtk_box_pack_start(GTK_BOX(object), view->target_entry, FALSE, FALSE, 5);

	gtk_table_attach_defaults(GTK_TABLE(table), object,  3,4,1,2);

	/* Output directory */
	field = gtk_label_new("Directory:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);

	view->destination_btn = gtk_file_chooser_button_new("Select output directory",
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(view->destination_btn), view->prefs->output_directory);
	gtk_table_attach_defaults(GTK_TABLE(table), view->destination_btn, 1,2,2,3);

	/* Output file */
	field = gtk_label_new("File:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,2,3);

	GtkWidget *path = gtk_hbox_new(FALSE, 0);
	view->run_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->run_entry), view->prefs->run_prefix);
	gtk_entry_set_width_chars(GTK_ENTRY(view->run_entry), 7);
	gtk_box_pack_start(GTK_BOX(path), view->run_entry, FALSE, FALSE, 0);

	field = gtk_label_new("-");
	gtk_box_pack_start(GTK_BOX(path), field, FALSE, FALSE, 0);

	view->frame_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(view->prefs->run_number, 0 , 1E9, 1, 10, 0), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(view->frame_entry), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(view->frame_entry), 4);
	gtk_box_pack_start(GTK_BOX(path), view->frame_entry, FALSE, FALSE, 0);

	/* Hack: Pad with spaces to ensure we don't reflow when switching object types */
	field = gtk_label_new(".fits.gz      ");
	gtk_box_pack_start(GTK_BOX(path), field, FALSE, FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), path, 3,4,2,3);

	/* Preview checkbox */
	view->display_checkbox = gtk_check_button_new_with_label("Preview");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->display_checkbox), TRUE);
	gtk_table_attach_defaults(GTK_TABLE(table), view->display_checkbox, 4,5,1,2);

	/* Save checkbox */
	view->save_checkbox = gtk_check_button_new_with_label("Save");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->save_checkbox), FALSE);
	g_signal_connect(view->save_checkbox, "toggled", G_CALLBACK(save_changed), view);
  	gtk_table_attach_defaults(GTK_TABLE(table), view->save_checkbox, 4,5,2,3);

	return frame;
}

/* Return a GtkWidget containing the camera panel */
GtkWidget *rangahau_camera_panel(RangahauView *view, void (startstop_pressed_cb)(GtkWidget *, void *))
{
	/* Frame around the panel */
	GtkWidget *frame = gtk_frame_new("Camera");

	/* Table */
	GtkWidget *table = gtk_table_new(4,3,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);
	GtkWidget *field;

	/* Status label */
	field = gtk_label_new("Status:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	view->camerastatus_label = gtk_label_new("");
	gtk_table_attach_defaults(GTK_TABLE(table), view->camerastatus_label, 1,3,0,1);


	/* Bin size */
	field = gtk_label_new("Bin Size:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);

	view->binsize_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(view->prefs->bin_size, 1, 1024, 1, 1, 0), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), view->binsize_entry, 1,2,1,2);
	gtk_entry_set_width_chars(GTK_ENTRY(view->binsize_entry), 3);
	field = gtk_label_new("pixel(s)");
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,1,2);
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);

	/* Exposure time */
	field = gtk_label_new("Exp. Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);

	view->exptime_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(view->prefs->exposure_time, 2, 10000, 1, 10, 0), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), view->exptime_entry, 1,2,2,3);
	gtk_entry_set_width_chars(GTK_ENTRY(view->exptime_entry), 3);
	field = gtk_label_new("seconds");
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,2,3);
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);

	/* Start / Stop button */
	view->startstop_btn = gtk_toggle_button_new_with_label("Start Acquisition");	
	gtk_table_attach_defaults(GTK_TABLE(table), view->startstop_btn, 3,4,0,3);
	g_signal_connect(view->startstop_btn, "clicked", G_CALLBACK(startstop_pressed_cb), NULL);

	/* Can't start acquisition until the camera is ready */
	gtk_widget_set_sensitive(view->startstop_btn, FALSE);

	return frame;
}

/* Return a GtkWidget containing the status panel */
GtkWidget *rangahau_status_panel(RangahauView *view)
{
	GtkWidget *frame = gtk_frame_new("GPS Timer");
	GtkWidget *table = gtk_table_new(2,3,FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	GtkWidget *field;


	field = gtk_label_new("Status:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	field = gtk_label_new("TODO");
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 1,2,0,1);

	field = gtk_label_new("GPS Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);
	view->gpstime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view->gpstime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view->gpstime_label, 1,2,1,2);

	field = gtk_label_new("PC Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);
	view->pctime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view->pctime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view->pctime_label, 1,2,2,3);

	return frame;
}

/* Return a GtkWidget containing the save settings panel */
GtkWidget *rangahau_save_panel(RangahauView *view)
{
	GtkWidget *frame = gtk_frame_new("Output");
	GtkWidget *box = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 0);

	GtkWidget *align = gtk_alignment_new(0,0.5,0,0);
	gtk_box_pack_start(GTK_BOX(box), align, FALSE, FALSE, 5);
	GtkWidget *path = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align), path);

	GtkWidget *item = gtk_label_new("Directory:");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 5);

	view->destination_btn = gtk_file_chooser_button_new("Select output directory",
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(view->destination_btn), "/home/sullivan/Desktop");
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(view->destination_btn), 30);	
	gtk_box_pack_start(GTK_BOX(path), view->destination_btn, FALSE, FALSE, 0);

	item = gtk_label_new("File:");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 5);

	view->run_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view->run_entry), "run");
	gtk_entry_set_width_chars(GTK_ENTRY(view->run_entry), 7);
	gtk_box_pack_start(GTK_BOX(path), view->run_entry, FALSE, FALSE, 0);

	item = gtk_label_new("-");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 0);

	view->frame_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(0, 0, 100000, 1, 10, 0), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(view->frame_entry), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(view->frame_entry), 4);
	gtk_box_pack_start(GTK_BOX(path), view->frame_entry, FALSE, FALSE, 0);

	item = gtk_label_new(".fits.gz");
	gtk_box_pack_start(GTK_BOX(path), item, FALSE, FALSE, 0);

	/* Display and view checkboxes */
	GtkWidget *box2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);

	view->save_checkbox = gtk_check_button_new_with_label ("Save");
	gtk_box_pack_start(GTK_BOX(box2), view->save_checkbox, TRUE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->save_checkbox), FALSE);

	view->display_checkbox = gtk_check_button_new_with_label("Preview");
	gtk_box_pack_start(GTK_BOX(box2), view->display_checkbox, TRUE, TRUE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->display_checkbox), TRUE);
    
	return frame;
}

/* update the various information fields */
gboolean update_gui_cb(gpointer data)
{
	RangahauView *view = (RangahauView *)data;

	/* Frame number */
	if (!gtk_editable_get_editable(GTK_EDITABLE(view->frame_entry)))
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(view->frame_entry), view->prefs->run_number);

	/* PC time */	
	char strtime[30];
	time_t t = time(NULL);
	strftime(strtime, 30, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	gtk_label_set_label(GTK_LABEL(view->pctime_label), strtime);

	/* GPS time */
	char *gpstime = "Unavailable";
	char gpsbuf[30];
	RangahauGPSTimestamp ts;
	if (rangahau_gps_get_gpstime(view->gps, 500, &ts))
	{
		sprintf(gpsbuf, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hours, ts.minutes, ts.seconds);
		gpstime = gpsbuf;
	}
	gtk_label_set_label(GTK_LABEL(view->gpstime_label), gpstime);

	/* Camera status */
	char *label;
	switch(view->camera->status)
	{
		default:		
		case INITIALISING:
			label = "Initialising";
		break;
		case IDLE:
			label = "Idle";
		break;
		case ACTIVE:
			label = "Active";
		break;
	}
	gtk_label_set_label(GTK_LABEL(view->camerastatus_label), label);

	/* Can't start/stop acquisition if the camera is initialising */
	boolean initialising = (view->camera->status == INITIALISING);
	if (initialising == gtk_widget_get_sensitive(view->startstop_btn))
		gtk_widget_set_sensitive(view->startstop_btn, !initialising);

	return TRUE;
}

void rangahau_init_gui(RangahauView *view, void (startstop_pressed_cb)(GtkWidget *, void *))
{
	/* Main window */
	view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request (GTK_WIDGET(view->window), 627, 280);
	gtk_window_set_resizable(GTK_WINDOW(view->window), FALSE);
	gtk_window_set_title(GTK_WINDOW(view->window), "Rangahau Data Acquisition System");
	gtk_container_set_border_width(GTK_CONTAINER(view->window), 5);

	g_signal_connect(view->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect_swapped(view->window, "delete-event", G_CALLBACK(gtk_widget_destroy), view->window);

	/* 
	 * Top row: Output
	 * Bottom row: Hardware & Camera
	 */
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(view->window), vbox);

	/* top row */
	GtkWidget *topbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), topbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(topbox), rangahau_output_panel(view), FALSE, FALSE, 5);


	/* bottom row */
	GtkWidget *bottombox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), bottombox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bottombox), rangahau_status_panel(view), FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bottombox), rangahau_camera_panel(view, startstop_pressed_cb), FALSE, FALSE, 5);
	
	/* Update the gui at 10Hz */
	g_timeout_add(100, update_gui_cb, view);

	/* Display and run */
	gtk_widget_show_all(view->window);
}

