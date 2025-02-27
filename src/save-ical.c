/*
    Gpredict: Real-time satellite tracking and orbit prediction program

    Copyright (C)  2001-2017  Alexandru Csete, OZ9AEC.

    Authors: Alexandru Csete <oz9aec@gmail.com>

    Comments, questions and bugreports should be submitted via
    http://sourceforge.net/projects/gpredict/
    More details can be found at the project home page:

            http://gpredict.oz9aec.net/

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, visit http://www.fsf.org/
*/
#ifdef HAVE_CONFIG_H
#include <build-config.h>
#endif
#include <gtk/gtk.h>

#include "gtk-sat-data.h"
#include "pass-to-txt.h"
#include "predict-tools.h"
#include "sat-cfg.h"
#include "sat-log.h"
#include "sat-pass-dialogs.h"
#include "gpredict-utils.h"
#include "save-ical.h"
#include "sgpsdp/sgp4sdp4.h"
#include "time-tools.h"

static void     file_changed(GtkWidget * widget, gpointer data);
static void     save_pass_ical_exec(GtkWidget * parent,
                               pass_t * pass, qth_t * qth,
                               const gchar * savedir, const gchar * savefile,
                               gint format, gchar* sat);
static void     save_passes_ical_exec(GtkWidget * parent,
                                 GSList * passes, qth_t * qth,
                                 const gchar * savedir, const gchar * savefile,
                                 gint format, gchar* sat);
static void     save_to_file(GtkWidget * parent, const gchar * fname,
                             const gchar * data);

enum pass_content_e {
    PASS_CONTENT_ALL = 0,
    PASS_CONTENT_TABLE,
    PASS_CONTENT_DATA,
};

enum passes_content_e {
    PASSES_CONTENT_FULL = 0,
    PASSES_CONTENT_SUM,
};

#define SAVE_FORMAT_ICS     1

/**
 * Save a satellite pass.
 *
 * @param toplevel Pointer to the parent dialogue window
 *
 * This function is called from the button click handler of the satellite pass
 * dialogue when the user presses the Save button.
 *
 * The function opens the Save Pass dialogue asking the user for where to save
 * the pass and in which format. When the user has made the required choices,
 * the function uses the lower level functions to save the pass information
 * to a file.
 *
 * \note All the relevant data are attached to the parent dialogue window.
 */
void save_pass_ical(GtkWidget * parent)
{
    GtkWidget      *dialog;
    GtkWidget      *grid;
    GtkWidget      *dirchooser;
    GtkWidget      *filchooser;
    GtkWidget      *label;
    gint            response;
    pass_t         *pass;
    qth_t          *qth;
    gchar          *sat;
    gchar          *savedir = NULL;
    gchar          *savefile;


    /* get data attached to parent */
    sat = (gchar *) g_object_get_data(G_OBJECT(parent), "sat");
    qth = (qth_t *) g_object_get_data(G_OBJECT(parent), "qth");
    pass = (pass_t *) g_object_get_data(G_OBJECT(parent), "pass");

    /* create the dialog */
    dialog =
        gtk_dialog_new_with_buttons(_("Save Pass Details"), GTK_WINDOW(parent),
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    "_Cancel", GTK_RESPONSE_REJECT,
                                    "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    /* create the table */
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    /* directory chooser */
    label = gtk_label_new(_("Save in folder:"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_START,
                 "valign", GTK_ALIGN_CENTER, NULL);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    dirchooser = gtk_file_chooser_button_new(_("Select a folder"),
                                             GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    savedir = sat_cfg_get_str(SAT_CFG_STR_PRED_SAVE_DIR);
    if (savedir)
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dirchooser),
                                            savedir);
        g_free(savedir);
    }
    else
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dirchooser),
                                            g_get_home_dir());
    }
    gtk_grid_attach(GTK_GRID(grid), dirchooser, 1, 0, 1, 1);

    /* file name */
    label = gtk_label_new(_("Save using file name:"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_START,
                 "valign", GTK_ALIGN_CENTER, NULL);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    filchooser = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(filchooser), 100);
    g_signal_connect(filchooser, "changed", G_CALLBACK(file_changed), dialog);
    gtk_grid_attach(GTK_GRID(grid), filchooser, 1, 1, 1, 1);

    /* use satellite name + orbit num as default; replace invalid characters
       with dash */
    savefile = g_strdup_printf("%s-%d", pass->satname, pass->orbit);
    savefile = g_strdelimit(savefile, " ", '-');
    savefile = g_strdelimit(savefile, "!?/\\()*&%$#@[]{}=+<>,.|:;", '_');
    gtk_entry_set_text(GTK_ENTRY(filchooser), savefile);
    g_free(savefile);

    gtk_widget_show_all(grid);
    gtk_container_add(GTK_CONTAINER
                      (gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);

    /* run the dialog */
    response = gtk_dialog_run(GTK_DIALOG(dialog));

    switch (response)
    {
        /* user clicked the save button */
    case GTK_RESPONSE_ACCEPT:

        /* get file and directory */
        savedir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dirchooser));
        savefile = g_strdup(gtk_entry_get_text(GTK_ENTRY(filchooser)));

        /* call saver */
        save_pass_ical_exec(dialog, pass, qth, savedir, savefile, SAVE_FORMAT_ICS,
                       sat);

        /* store new settings */
        sat_cfg_set_str(SAT_CFG_STR_PRED_SAVE_DIR, savedir);

        /* clean up */
        g_free(savedir);
        g_free(savefile);
        break;

        /* cancel */
    default:
        break;

    }

    gtk_widget_destroy(dialog);
}

/**
 * Save a satellite passes.
 *
 * @param toplevel Pointer to the parent dialogue window
 *
 * This function is called from the button click handler of the satellite passes
 * dialogue when the user presses the Save button.
 *
 * The function opens the Save Pass dialogue asking the user for where to save
 * the data and in which format. When the user has made the required choices,
 * the function uses the lower level functions to save the pass information
 * to a file.
 *
 * @note All the relevant data are attached to the parent dialogue window.
 */
void save_passes_ical(GtkWidget * parent)
{
    GtkWidget      *dialog;
    GtkWidget      *grid;
    GtkWidget      *dirchooser;
    GtkWidget      *filchooser;
    GtkWidget      *label;
    gint            response;
    GSList         *passes;
    gchar          *sat;
    qth_t          *qth;
    gchar          *savedir = NULL;
    gchar          *savefile;

    /* get data attached to parent */
    sat = (gchar *) g_object_get_data(G_OBJECT(parent), "sat");
    qth = (qth_t *) g_object_get_data(G_OBJECT(parent), "qth");
    passes = (GSList *) g_object_get_data(G_OBJECT(parent), "passes");

    /* create the dialog */
    dialog = gtk_dialog_new_with_buttons(_("Save Passes in iCalendar format"), GTK_WINDOW(parent),
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Cancel", GTK_RESPONSE_REJECT,
                                         "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    /* create the table */
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    /* directory chooser */
    label = gtk_label_new(_("Save in folder:"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_START,
                 "valign", GTK_ALIGN_CENTER, NULL);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    dirchooser = gtk_file_chooser_button_new(_("Select a folder"),
                                             GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    savedir = sat_cfg_get_str(SAT_CFG_STR_PRED_SAVE_DIR);
    if (savedir)
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dirchooser),
                                            savedir);
        g_free(savedir);
    }
    else
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dirchooser),
                                            g_get_home_dir());
    }
    gtk_grid_attach(GTK_GRID(grid), dirchooser, 1, 0, 1, 1);

    /* file name */
    label = gtk_label_new(_("Save using file name:"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_START,
                 "valign", GTK_ALIGN_CENTER, NULL);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    filchooser = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(filchooser), 100);
    g_signal_connect(filchooser, "changed", G_CALLBACK(file_changed), dialog);
    gtk_grid_attach(GTK_GRID(grid), filchooser, 1, 1, 1, 1);

    /* use satellite name + orbit num as default; replace invalid characters
       with dash */
    savefile = g_strdup_printf("%s-passes", sat);
    savefile = g_strdelimit(savefile, " ", '-');
    savefile = g_strdelimit(savefile, "!?/\\()*&%$#@[]{}=+<>,.|:;", '_');
    gtk_entry_set_text(GTK_ENTRY(filchooser), savefile);
    g_free(savefile);

    gtk_widget_show_all(grid);
    gtk_container_add(GTK_CONTAINER
                      (gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);

    /* run the dialog */
    response = gtk_dialog_run(GTK_DIALOG(dialog));

    switch (response)
    {
        /* user clicked the save button */
    case GTK_RESPONSE_ACCEPT:

        /* get file and directory */
        savedir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dirchooser));
        savefile = g_strdup(gtk_entry_get_text(GTK_ENTRY(filchooser)));

        /* call saver */
        save_passes_ical_exec(dialog, passes, qth, savedir, savefile,
                         SAVE_FORMAT_ICS, sat);

        /* store new settings */
        sat_cfg_set_str(SAT_CFG_STR_PRED_SAVE_DIR, savedir);

        /* clean up */
        g_free(savedir);
        g_free(savefile);
        break;

        /* cancel */
    default:
        break;
    }
    gtk_widget_destroy(dialog);
}

/**
 * Manage file name changes.
 *
 * @param widget The GtkEntry that received the signal
 * @param data User data (not used).
 *
 * This function is called when the contents of the file name entry changes.
 * It validates all characters and invalid chars are deleted.
 * The function sets the state of the Save button according to the validity
 * of the current file name.
 */
static void file_changed(GtkWidget * widget, gpointer data)
{
    gchar          *entry, *end, *j;
    gint            len, pos;
    const gchar    *text;
    GtkWidget      *dialog = GTK_WIDGET(data);

    /* ensure that only valid characters are entered
       (stolen from xlog, tnx pg4i)
     */
    entry = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    if ((len = g_utf8_strlen(entry, -1)) > 0)
    {
        end = entry + g_utf8_strlen(entry, -1);
        for (j = entry; j < end; ++j)
        {
            if (!gpredict_legal_char(*j))
            {
                gdk_display_beep(gdk_display_get_default());
                pos = gtk_editable_get_position(GTK_EDITABLE(widget));
                gtk_editable_delete_text(GTK_EDITABLE(widget), pos, pos + 1);
            }
        }
    }

    /* step 2: if name seems all right, enable OK button */
    text = gtk_entry_get_text(GTK_ENTRY(widget));

    if (g_utf8_strlen(text, -1) > 0)
    {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                          GTK_RESPONSE_ACCEPT, TRUE);
    }
    else
    {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                          GTK_RESPONSE_ACCEPT, FALSE);
    }
}

/**
 * Save data as iCalendar file.
 *
 * @param parent Parent window (needed for error dialogs).
 * @param pass The pass data to save.
 * @param qth The observer data
 * @param savedir The directory where data should be saved.
 * @param savefile The file where data should be saved.
 * @param format The file format
 * @param contents The contents defining whether to save headers and such.
 *
 * This is the function that does the actual saving to a data file once all
 * required information has been gathered (i.e. file name, format, contents).
 * The function does some last minute checking while saving and provides
 * error messages if anything fails during the process.
 *
 * If the time zone is UTC, it is specified in the final iCal file.
 * If local time is used, no time zone is specified for lack of a good way
 * to determine the users time zone.
 *
 */
static void save_passes_ical_exec(GtkWidget * parent,
                             GSList * passes, qth_t * qth,
                             const gchar * savedir, const gchar * savefile,
                             gint format, gchar *sat)
{
    gchar          *fname;
    gchar          *buff = NULL;
    gchar          *data = NULL;
    pass_t         *pass;
    gboolean 	    loc;
    gchar          *timezone;

    switch (format)
    {
    case SAVE_FORMAT_ICS:

        /* prepare full file name */
        fname =
            g_strconcat(savedir, G_DIR_SEPARATOR_S, savefile, ".ics", NULL);


	/* determine time zone */
	loc = sat_cfg_get_bool(SAT_CFG_BOOL_USE_LOCAL_TIME);
	if(loc){
		timezone = g_strdup_printf(":");
	} else {
		timezone = g_strdup(";TZID=UTC:");
	}


        /* create file contents */
        
	/* header */
	data = g_strdup_printf("BEGIN:VCALENDAR\nVERSION:2.0\nCALSCALE:GREGORIAN\n");
	
	/* events */
	gchar *fmtstr;
	gchar tbuff[TIME_FORMAT_MAX_LENGTH];	
	fmtstr = "%Y%m%dT%H%M%S";

	guint n = g_slist_length(passes);
	for(guint i = 0; i < n; i++)
	{
		buff = g_strdup(data);
		g_free(data);
		data = g_strdup_printf("%sBEGIN:VEVENT\n", buff);
		g_free(buff);
		pass = PASS(g_slist_nth_data(passes, i));

		/* AOS */
		daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, fmtstr, pass->aos);
		buff = g_strdup(data);
		g_free(data);
		data = g_strdup_printf("%sDTSTART%s%s\n", buff, timezone, tbuff);
		g_free(buff);

		/* LOS */
		daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, fmtstr, pass->los);
		buff = g_strdup(data);
		g_free(data);
		data = g_strdup_printf("%sDTEND%s%s\n", buff, timezone, tbuff);
		g_free(buff);

		/* Summary with Max Elevation and Sat name */
		buff = g_strdup_printf("%sSUMMARY:%s [%.0f°]\n", data, sat, pass->max_el);
		g_free(data);
		data = g_strdup(buff);
		g_free(buff);

		/* UID */
		/* UID is formatted as:
		 * <Satname><Orbit number><Date and hour of pass>@<Latitude><Longitude> */
		daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, "%Y%m%d%H", pass->aos);
		buff = g_strdup(data);
		g_free(data);
		data = g_strdup_printf("%sUID:%s%d%s@%f%f\n", buff, sat, pass->orbit, tbuff, qth->lat, qth->lon);
		g_free(buff);

		/* Description */
		gchar *line;
		
		/* Duration */
		guint h, m, s;
		/* convert julian date to seconds */
		s = (guint) ((pass->los - pass->aos)*86400);
		/* extract hours */
		h = (guint) floor(s / 3600);
		s = s - 3600*h;
		/*extract minutes */
		m = (guint) floor(s/60);
		s = s - 60*m;
		
		line = g_strdup_printf("Duration: %02d:%02d\\n", m, s);
		
		/* AOS Az */
	        buff = g_strdup_printf("%sAOS Azimuth:  %6.2f\\n", line, pass->aos_az);
           	g_free(line);
            	line = g_strdup(buff);
            	g_free(buff);	
		
		/* LOS Az */
	        buff = g_strdup_printf("%sLOS Azimuth:  %6.2f\\n", line, pass->los_az);
           	g_free(line);
            	line = g_strdup(buff);
            	g_free(buff);
		
		buff = g_strdup_printf("%sDESCRIPTION:%s\n", data, line);
		g_free(data);
		data = g_strdup(buff);
		g_free(buff);

		buff = g_strdup_printf("%sEND:VEVENT\n", data);
		g_free(data);
		data = g_strdup(buff);
		g_free(buff);
	}

	buff = g_strdup_printf("%sEND:VCALENDAR\n", data);
	g_free(data);
	data = g_strdup(buff);
	g_free(buff);


        /* save data */
        save_to_file(parent, fname, data);
        g_free(data);
        g_free(fname);
        break;

    default:
        sat_log_log(SAT_LOG_LEVEL_ERROR,
                    _("%s: Invalid file format: %d"), __func__, format);
        break;
    }
}

/**
 * Save data to file.
 *
 * @param parent Parent window (needed for error dialogs).
 * @param passes The pass data to save.
 * @param qth The observer data
 * @param savedir The directory where data should be saved.
 * @param savefile The file where data should be saved.
 * @param format The file format
 * @param contents The contents defining whether to save headers and such.
 *
 * This is the function that does the actual saving to a data file once all
 * required information has been gathered (i.e. file name, format, contents).
 * The function does some last minute checking while saving and provides
 * error messages if anything fails during the process.
 *
 * @note The formatting is done by external functions according to the selected
 *       file format.
 */
static void save_pass_ical_exec(GtkWidget * parent,
                           pass_t * pass, qth_t * qth,
                           const gchar * savedir, const gchar * savefile,
                           gint format, gchar* sat)
{
    gchar          *fname;
    gchar          *buff = NULL;
    gchar          *data = NULL;
    gchar          *timezone;
    gboolean 	    loc;


    switch (format)
    {
    case SAVE_FORMAT_ICS:

        /* prepare full file name */
        fname = g_strconcat(savedir, G_DIR_SEPARATOR_S, savefile, ".ics", NULL);

	/* determine time zone */
	loc = sat_cfg_get_bool(SAT_CFG_BOOL_USE_LOCAL_TIME);
	if(loc){
		timezone = g_strdup_printf(":");
	} else {
		timezone = g_strdup(";TZID=UTC:");
	}


	/* create file contents */
        
	/* header */
	data = g_strdup_printf("BEGIN:VCALENDAR\nVERSION:2.0\nCALSCALE:GREGORIAN\n");
	
	/* events */
	gchar *fmtstr;
	gchar tbuff[TIME_FORMAT_MAX_LENGTH];	
	fmtstr = "%Y%m%dT%H%M%S";

	buff = g_strdup(data);
	g_free(data);
	data = g_strdup_printf("%sBEGIN:VEVENT\n", buff);
	g_free(buff);

	/* AOS */
	daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, fmtstr, pass->aos);
	buff = g_strdup(data);
	g_free(data);
	data = g_strdup_printf("%sDTSTART%s%s\n", buff, timezone, tbuff);
	g_free(buff);

	/* LOS */
	daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, fmtstr, pass->los);
	buff = g_strdup(data);
	g_free(data);
	data = g_strdup_printf("%sDTEND%s%s\n", buff, timezone, tbuff);
	g_free(buff);

	/* Summary with Max Elevation and Sat name */
	buff = g_strdup_printf("%sSUMMARY:%s [%.0f°]\n", data, sat, pass->max_el);
	g_free(data);
	data = g_strdup(buff);
	g_free(buff);

	/* UID */
	/* UID is formatted as:
	 * <Satname><Orbit number><Date and hour of pass>@<Latitude><Longitude> */
	daynum_to_str(tbuff, TIME_FORMAT_MAX_LENGTH, "%Y%m%d%H", pass->aos);
	buff = g_strdup(data);
	g_free(data);
	data = g_strdup_printf("%sUID:%s%d%s@%f%f\n", buff, sat, pass->orbit, tbuff, qth->lat, qth->lon);
	g_free(buff);

	/* Description */
	gchar *line;
	
	/* Duration */
	guint h, m, s;
	/* convert julian date to seconds */
	s = (guint) ((pass->los - pass->aos)*86400);
	/* extract hours */
	h = (guint) floor(s / 3600);
	s = s - 3600*h;
	/*extract minutes */
	m = (guint) floor(s/60);
	s = s - 60*m;
	
	line = g_strdup_printf("Duration: %02d:%02d\\n", m, s);
	
	/* AOS Az */
	buff = g_strdup_printf("%sAOS Azimuth:  %6.2f\\n", line, pass->aos_az);
	g_free(line);
	line = g_strdup(buff);
	g_free(buff);	
	
	/* LOS Az */
	buff = g_strdup_printf("%sLOS Azimuth:  %6.2f\\n", line, pass->los_az);
	g_free(line);
	line = g_strdup(buff);
	g_free(buff);
	
	buff = g_strdup_printf("%sDESCRIPTION:%s\n", data, line);
	g_free(data);
	data = g_strdup(buff);
	g_free(buff);

	buff = g_strdup_printf("%sEND:VEVENT\n", data);
	g_free(data);
	data = g_strdup(buff);
	g_free(buff);
	

	buff = g_strdup_printf("%sEND:VCALENDAR\n", data);
	g_free(data);
	data = g_strdup(buff);
	g_free(buff);

        

        /* save data */
        save_to_file(parent, fname, data);

        /* clean up memory */
        g_free(fname);
        g_free(data);

        break;

    default:
        sat_log_log(SAT_LOG_LEVEL_ERROR,
                    _("%s: Invalid file format: %d"), __func__, format);
        break;
    }
}

static void save_to_file(GtkWidget * parent, const gchar * fname,
                         const gchar * data)
{
    GIOChannel     *chan;
    GError         *err = NULL;
    GtkWidget      *dialog;
    gsize           count;

    /* create file */
    chan = g_io_channel_new_file(fname, "w", &err);
    if (err != NULL)
    {
        sat_log_log(SAT_LOG_LEVEL_ERROR,
                    _("%s: Could not create file %s (%s)"),
                    __func__, fname, err->message);

        /* error dialog */
        dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Could not create file %s\n\n%s"),
                                        fname, err->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        /* clean up and return */
        g_clear_error(&err);

        return;
    }

    /* save contents to file */
    g_io_channel_write_chars(chan, data, -1, &count, &err);
    if (err != NULL)
    {
        sat_log_log(SAT_LOG_LEVEL_ERROR,
                    _("%s: An error occurred while saving data to %s (%s)"),
                    __func__, fname, err->message);

        dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _
                                        ("An error occurred while saving data to %s\n\n%s"),
                                        fname, err->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_clear_error(&err);
    }
    else
    {
        sat_log_log(SAT_LOG_LEVEL_DEBUG,
                    _("%s: Written %d characters to %s"),
                    __func__, count, fname);
    }

    /* close file, we don't care about errors here */
    g_io_channel_shutdown(chan, TRUE, NULL);
    g_io_channel_unref(chan);

}
