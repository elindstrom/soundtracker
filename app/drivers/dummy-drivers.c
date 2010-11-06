
/*
 * The Real SoundTracker - Dummy drivers
 *
 * Copyright (C) 2001 Michael Krause
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "i18n.h"
#include "driver-in.h"
#include "driver-out.h"

typedef struct dummy_driver {
    GtkWidget *configwidget;
} dummy_driver;

static void
dummy_make_config_widgets (dummy_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("No driver available for your system."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
dummy_getwidget (void *dp)
{
    dummy_driver * const d = dp;

    return d->configwidget;
}

static void *
dummy_new (void)
{
    dummy_driver *d = g_new(dummy_driver, 1);

    dummy_make_config_widgets(d);

    return d;
}

static void
dummy_destroy (void *dp)
{
    dummy_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_free(dp);
}

static void
dummy_release (void *dp)
{
}

static gboolean
dummy_open (void *dp)
{
    return FALSE;
}

st_out_driver driver_out_dummy = {
    { "No Output",

      dummy_new,
      dummy_destroy,

      dummy_open,
      dummy_release,

      dummy_getwidget,
    },
};

st_out_driver driver_in_dummy = {
    { "No Input",

      dummy_new,
      dummy_destroy,

      dummy_open,
      dummy_release,

      dummy_getwidget,
    },
};

