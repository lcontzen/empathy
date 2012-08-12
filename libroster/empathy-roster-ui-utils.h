/*
 * Copyright (C) 2002-2007 Imendio AB
 * Copyright (C) 2007-2010 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef _EMPATHY_ROSTER_UI_UTILS_H_
#define _EMPATHY_ROSTER_UI_UTILS_H_

#include <gtk/gtk.h>
#include <telepathy-glib/telepathy-glib.h>

gboolean empathy_individual_match_string (
    FolksIndividual *individual,
    const gchar *text,
    GPtrArray *words);

const gchar * empathy_icon_name_for_presence (
    TpConnectionPresenceType presence);

const gchar * empathy_icon_name_for_individual (FolksIndividual *individual);

void empathy_pixbuf_avatar_from_individual_scaled_async (
    FolksIndividual *individual,
    gint width,
    gint height,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

GdkPixbuf * empathy_pixbuf_avatar_from_individual_scaled_finish (
    FolksIndividual *individual,
    GAsyncResult *result,
    GError **error);

GdkPixbuf * empathy_pixbuf_from_icon_name_sized (const gchar *icon_name,
    gint size);

#endif /*  _EMPATHY_ROSTER_UI_UTILS_H_ */
