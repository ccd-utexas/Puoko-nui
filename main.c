/*
* Copyright 2007-2010 The Authors (see AUTHORS)
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <time.h>

typedef struct
{
	/* Aquire panel */
	GtkWidget *startstop_btn;

	/* Hardware panel */
	GtkWidget *gpstime_label;
	GtkWidget *pctime_label;

	/* Settings panel */
	GtkWidget *observers_entry;
	GtkWidget *exptime_entry;
	GtkWidget *target_combobox;
	GtkWidget *target_entry;

	/* Destination panel */
	GtkWidget *destination_entry;
	GtkWidget *destination_btn;
	GtkWidget *run_entry;
	GtkWidget *frame_entry;

} RangahauView;

RangahauView view;

/* Convinience function */
make_gtk_entry_editable(GtkWidget *widget, gboolean editable)
{
		gtk_editable_set_editable(GTK_EDITABLE(widget), editable);
		gtk_widget_set_sensitive(widget, editable);
}
/*
 * Start or stop aquiring frames in response to user input
 */
static void startstop_pressed(GtkWidget *widget, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) 
    {
        g_print("Starting Aquisition\n");
		/* Disable settings fields */
		make_gtk_entry_editable(view.observers_entry, FALSE);
		make_gtk_entry_editable(view.exptime_entry, FALSE);
		gtk_widget_set_sensitive(view.target_combobox, FALSE);
		make_gtk_entry_editable(view.target_entry, FALSE);

		/* Disable destination fields */
		make_gtk_entry_editable(view.destination_entry, FALSE);
		gtk_widget_set_sensitive(view.destination_btn, FALSE);
		make_gtk_entry_editable(view.run_entry, FALSE);
		make_gtk_entry_editable(view.frame_entry, FALSE);

		gtk_button_set_label(GTK_BUTTON(widget), "Stop Aquisition");
    }
	else
	{
    	g_print("Stopping Aquisition\n");
		/* Enable settings fields */
		make_gtk_entry_editable(view.observers_entry, TRUE);
		make_gtk_entry_editable(view.exptime_entry, TRUE);
		gtk_widget_set_sensitive(view.target_combobox, TRUE);
		make_gtk_entry_editable(view.target_entry, TRUE);

		/* Enable destination fields */
		make_gtk_entry_editable(view.destination_entry, TRUE);
		gtk_widget_set_sensitive(view.destination_btn, TRUE);
		make_gtk_entry_editable(view.run_entry, TRUE);
		make_gtk_entry_editable(view.frame_entry, TRUE);

		gtk_button_set_label(GTK_BUTTON(widget), "Start Aquisition");
    }
}

/* Set the pctime text field */
gboolean update_pctime_cb(gpointer data)
{
	char strtime[20];
	time_t t = time(NULL);
	strftime(strtime, 20, "%Y-%m-%d %H:%M:%S", gmtime(&t));
	gtk_label_set_label(GTK_LABEL(view.pctime_label), strtime);
}

/* Return a GtkWidget containing the settings panel */
GtkWidget *rangahau_settings_panel()
{
	/* Frame around the panel */
	GtkWidget *frame = gtk_frame_new("Settings");
	
	/* Table */
	GtkWidget *table = gtk_table_new(5,3,FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	GtkWidget *field;
	
	/* Observatory, Telescope, Instrument are set
     * in an external data file */

	/* Observers */
	field = gtk_label_new("Observers:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	view.observers_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view.observers_entry), "DJS");
	gtk_entry_set_width_chars(GTK_ENTRY(view.observers_entry), 12);
	gtk_table_attach_defaults(GTK_TABLE(table), view.observers_entry, 1,3,0,1);

	/* Exposure time */
	field = gtk_label_new("Exp. Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);

	GtkAdjustment *adj = (GtkAdjustment *)gtk_adjustment_new(5, 2, 10000, 1, 10, 0);
	view.exptime_entry = gtk_spin_button_new(adj, 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), view.exptime_entry, 1,2,1,2);
	gtk_entry_set_width_chars(GTK_ENTRY(view.exptime_entry), 3);

	field = gtk_label_new("seconds");
	gtk_table_attach_defaults(GTK_TABLE(table), field, 2,3,1,2);
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);

	/* Observation type (dark, flat, target)
	 * Includes a dropdown for type, and an entry for setting
     * the target name */
	field = gtk_label_new("Type:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);

	view.target_combobox = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), view.target_combobox, 1,2,2,3);
	gtk_combo_box_append_text(GTK_COMBO_BOX(view.target_combobox), "Dark");
	gtk_combo_box_append_text(GTK_COMBO_BOX(view.target_combobox), "Flat");
	gtk_combo_box_append_text(GTK_COMBO_BOX(view.target_combobox), "Target");
	gtk_combo_box_set_active(GTK_COMBO_BOX(view.target_combobox), 2);

	view.target_entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), view.target_entry, 2,3,2,3);
	gtk_entry_set_text(GTK_ENTRY(view.target_entry), "ec20058");
	gtk_entry_set_width_chars(GTK_ENTRY(view.target_entry), 8);

	return frame;
}

/* Return a GtkWidget containing the hardware panel */
GtkWidget *rangahau_hardware_panel()
{
	GtkWidget *frame = gtk_frame_new("Hardware");
	GtkWidget *table = gtk_table_new(4,2,FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	GtkWidget *field = gtk_label_new("Camera:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,0,1);
	field = gtk_label_new("Idle");
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 1,2,0,1);


	field = gtk_label_new("GPS:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,1,2);
	field = gtk_label_new("Locked");
	gtk_misc_set_alignment(GTK_MISC(field), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 1,2,1,2);


	field = gtk_label_new("GPS Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,2,3);
	view.gpstime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view.gpstime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view.gpstime_label, 1,2,2,3);

	field = gtk_label_new("PC Time:");
	gtk_misc_set_alignment(GTK_MISC(field), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), field, 0,1,3,4);
	view.pctime_label = gtk_label_new("2010-12-10 04:47:57");
	gtk_misc_set_alignment(GTK_MISC(view.pctime_label), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), view.pctime_label, 1,2,3,4);

	return frame;
}

/* Return a GtkWidget containing the save settings panel */
GtkWidget *rangahau_save_panel()
{
	GtkWidget *frame = gtk_frame_new("Destination");
	GtkWidget *box = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);

	view.destination_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view.destination_entry), "/home/sullivan/Desktop");
	gtk_entry_set_width_chars(GTK_ENTRY(view.destination_entry), 27);
	gtk_box_pack_start(GTK_BOX(box), view.destination_entry, FALSE, FALSE, 0);

	view.destination_btn = gtk_button_new_with_label("Browse");
	gtk_box_pack_start(GTK_BOX(box), view.destination_btn, FALSE, FALSE, 0);

	GtkWidget *item = gtk_label_new("/");
	gtk_box_pack_start(GTK_BOX(box), item, FALSE, FALSE, 0);

	view.run_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(view.run_entry), "run");
	gtk_entry_set_width_chars(GTK_ENTRY(view.run_entry), 7);
	gtk_box_pack_start(GTK_BOX(box), view.run_entry, FALSE, FALSE, 0);

	item = gtk_label_new("-");
	gtk_box_pack_start(GTK_BOX(box), item, FALSE, FALSE, 0);

	view.frame_entry = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(0, 0, 100000, 1, 10, 0), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(view.frame_entry), TRUE);
	gtk_entry_set_width_chars(GTK_ENTRY(view.frame_entry), 4);
	gtk_box_pack_start(GTK_BOX(box), view.frame_entry, FALSE, FALSE, 0);

	item = gtk_label_new(".fits");
	gtk_box_pack_start(GTK_BOX(box), item, FALSE, FALSE, 0);

	return frame;
}

/* Return a GtkWidget containing the aquisition settings panel */
GtkWidget *rangahau_aquire_panel()
{
	/* Setup */	
	GtkWidget *align = gtk_alignment_new(0,1,0,0);	
	GtkWidget *box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(align), box);

	/* Contents */
    GtkWidget *display = gtk_check_button_new_with_label("Display Frames");
    gtk_box_pack_start(GTK_BOX(box), display, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(display), TRUE);
    
	GtkWidget *save = gtk_check_button_new_with_label ("Save Frames");
    gtk_box_pack_start(GTK_BOX(box), save, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save), FALSE);

	view.startstop_btn = gtk_toggle_button_new_with_label("Start Aquisition");	
	gtk_box_pack_start(GTK_BOX(box), view.startstop_btn, FALSE, FALSE, 10);
    g_signal_connect(view.startstop_btn, "clicked", G_CALLBACK (startstop_pressed), NULL);



	return align;
}

int main( int argc, char *argv[] )
{
    gtk_init (&argc, &argv);

	/* Main window */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET(window), 650, 210);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_title(GTK_WINDOW(window), "Rangahau Data Aquisition System");
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);

    g_signal_connect(window, "destroy",
                      G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(window, "delete-event",
                              G_CALLBACK(gtk_widget_destroy), 
                              window);

	/* 
	 * Outer container
	 * Left column: All settings
	 * Right column: Aquire settings
	 */
    GtkWidget *outer = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), outer);

	/* 
	 * Inner-left container.
	 * Top row: Hardware & Settings
	 * Bottom row: Save path
	 */
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(outer), vbox, FALSE, FALSE, 0);

	/* top row */
	GtkWidget *topbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), topbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(topbox), rangahau_hardware_panel(), FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(topbox), rangahau_settings_panel(), FALSE, FALSE, 5);

	/* bottom row */
	GtkWidget *bottombox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), bottombox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bottombox), rangahau_save_panel(), FALSE, FALSE, 5);

	/* 
	 * Inner-right container.
	 */
	gtk_box_pack_end(GTK_BOX(outer), rangahau_aquire_panel(), FALSE, FALSE, 5);


	/* Update the pctime at 10Hz */
	g_timeout_add(100,update_pctime_cb,NULL);

	/* Display and run */
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
