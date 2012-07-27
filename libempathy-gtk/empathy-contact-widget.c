/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#ifdef HAVE_LIBCHAMPLAIN
#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#endif

#include <telepathy-glib/account.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>

#include <libempathy/empathy-location.h>
#include <libempathy/empathy-time.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-client-factory.h>

#include "empathy-calendar-button.h"
#include "empathy-contact-widget.h"
#include "empathy-contactinfo-utils.h"
#include "empathy-account-chooser.h"
#include "empathy-avatar-chooser.h"
#include "empathy-avatar-image.h"
#include "empathy-groups-widget.h"
#include "empathy-ui-utils.h"
#include "empathy-string-parser.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>

/**
 * SECTION:empathy-contact-widget
 * @title:EmpathyContactWidget
 * @short_description: A widget used to display and edit details about a contact
 * @include: libempathy-empathy-contact-widget.h
 *
 * #EmpathyContactWidget is a widget which displays appropriate widgets
 * with details about a contact, also allowing changing these details,
 * if desired.
 */

/**
 * EmpathyContactWidget:
 * @parent: parent object
 *
 * Widget which displays appropriate widgets with details about a contact,
 * also allowing changing these details, if desired.
 */

G_DEFINE_TYPE (EmpathyContactWidget, empathy_contact_widget, GTK_TYPE_BOX)

/* Delay before updating the widget when the id entry changed (seconds) */
#define ID_CHANGED_TIMEOUT 1

#define DATA_FIELD "contact-info-field"

struct _EmpathyContactWidgetPriv
{
  EmpathyContact *contact;
  EmpathyContactWidgetFlags flags;
  guint widget_id_timeout;
  gulong fav_sig_id;

  /* Contact */
  GtkWidget *widget_avatar;
  GtkWidget *widget_account;
  GtkWidget *image_account;
  GtkWidget *label_account;
  GtkWidget *widget_id;
  GtkWidget *widget_alias;
  GtkWidget *label_alias;
  GtkWidget *hbox_presence;
  GtkWidget *image_state;
  GtkWidget *label_status;
  GtkWidget *grid_contact;
  GtkWidget *vbox_avatar;
  GtkWidget *favourite_checkbox;
  GtkWidget *label_details;
  GtkWidget *label_left_account;

  /* Location */
  GtkWidget *vbox_location;
  GtkWidget *subvbox_location;
  GtkWidget *grid_location;
  GtkWidget *label_location;
#ifdef HAVE_LIBCHAMPLAIN
  GtkWidget *viewport_map;
  GtkWidget *map_view_embed;
  ChamplainView *map_view;
#endif

  /* Groups */
  GtkWidget *groups_widget;

  /* Details */
  GtkWidget *hbox_details_requested;
  GtkWidget *spinner_details;
  GList *details_to_set;
  GCancellable *details_cancellable;
  gboolean details_changed;

  /* Client */
  GtkWidget *vbox_client;
  GtkWidget *grid_client;
  GtkWidget *hbox_client_requested;
};

typedef struct
{
  EmpathyContactWidget *self;
  const gchar *name;
  gboolean found;
  GtkTreeIter found_iter;
} FindName;

enum
{
  COL_NAME,
  COL_ENABLED,
  COL_EDITABLE,
  COL_COUNT
};

static gboolean
field_value_is_empty (TpContactInfoField *field)
{
  guint i;

  if (field->field_value == NULL)
    return TRUE;

  /* Field is empty if all its values are empty */
  for (i = 0; field->field_value[i] != NULL; i++)
    {
      if (!tp_str_empty (field->field_value[i]))
        return FALSE;
    }

  return TRUE;
}

static void
set_contact_info_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;

  if (!tp_connection_set_contact_info_finish (TP_CONNECTION (source), result,
        &error))
    {
      DEBUG ("SetContactInfo() failed: %s", error->message);
      g_error_free (error);
      return;
    }

  DEBUG ("SetContactInfo() succeeded");
}

static void
contact_widget_save (EmpathyContactWidget *self)
{
  TpConnection *connection;
  GList *l, *next;

  connection = empathy_contact_get_connection (self->priv->contact);

  /* Remove empty fields */
  for (l = self->priv->details_to_set; l != NULL; l = next)
    {
      TpContactInfoField *field = l->data;

      next = l->next;
      if (field_value_is_empty (field))
        {
          DEBUG ("Drop empty field: %s", field->field_name);
          tp_contact_info_field_free (field);
          self->priv->details_to_set =
              g_list_delete_link (self->priv->details_to_set, l);
        }
    }

  if (self->priv->details_to_set != NULL)
    {
      if (self->priv->details_changed)
        {
          tp_connection_set_contact_info_async (connection,
              self->priv->details_to_set, set_contact_info_cb, NULL);
        }

      tp_contact_info_list_free (self->priv->details_to_set);
      self->priv->details_to_set = NULL;
    }
}

static void
contact_widget_details_setup (EmpathyContactWidget *self)
{
  gtk_widget_hide (self->priv->label_details);

  self->priv->spinner_details = gtk_spinner_new ();
  gtk_box_pack_end (GTK_BOX (self->priv->hbox_details_requested),
      self->priv->spinner_details, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->spinner_details);
}

static void
contact_widget_details_changed_cb (GtkEntry *entry,
    EmpathyContactWidget *self)
{
  const gchar *strv[] = { NULL, NULL };
  TpContactInfoField *field;

  self->priv->details_changed = TRUE;

  field = g_object_get_data ((GObject *) entry, DATA_FIELD);
  g_assert (field != NULL);

  strv[0] = gtk_entry_get_text (entry);

  if (field->field_value != NULL)
    g_strfreev (field->field_value);
  field->field_value = g_strdupv ((GStrv) strv);
}

static void
contact_widget_bday_changed_cb (EmpathyCalendarButton *button,
    GDate *date,
    EmpathyContactWidget *self)
{
  const gchar *strv[] = { NULL, NULL };
  TpContactInfoField *field;

  self->priv->details_changed = TRUE;

  field = g_object_get_data ((GObject *) button, DATA_FIELD);
  g_assert (field != NULL);

  if (date != NULL)
    {
      gchar tmp[255];

      g_date_strftime (tmp, sizeof (tmp), EMPATHY_DATE_FORMAT_DISPLAY_SHORT,
          date);
      strv[0] = tmp;
    }

  if (field->field_value != NULL)
    g_strfreev (field->field_value);
  field->field_value = g_strdupv ((GStrv) strv);
}

static void contact_widget_details_notify_cb (EmpathyContactWidget *self);

static gboolean
field_name_in_field_list (GList *list,
    const gchar *name)
{
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      TpContactInfoField *field = l->data;

      if (!tp_strdiff (field->field_name, name))
        return TRUE;
    }

  return FALSE;
}

static TpContactInfoFieldSpec *
get_spec_from_list (GList *list,
    const gchar *name)
{
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      TpContactInfoFieldSpec *spec = l->data;

      if (!tp_strdiff (spec->name, name))
        return spec;
    }

  return NULL;
}

static void
add_row (GtkGrid *grid,
    GtkWidget *title,
    GtkWidget *value)
{
  gtk_grid_attach_next_to (grid, title, NULL, GTK_POS_BOTTOM, 2, 1);
  gtk_misc_set_alignment (GTK_MISC (title), 1, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (title),
      GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_show (title);

  g_object_set_data (G_OBJECT (title), "added-row", (gpointer) TRUE);

  gtk_grid_attach_next_to (grid, value, title, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_hexpand (value, TRUE);

  if (GTK_IS_MISC (value))
    gtk_misc_set_alignment (GTK_MISC (value), 0, 0.5);

  gtk_widget_show (value);

  g_object_set_data (G_OBJECT (value), "added-row", (gpointer) TRUE);
}

static guint
contact_widget_details_update_edit (EmpathyContactWidget *self)
{
  TpContact *contact;
  TpConnection *connection;
  GList *specs, *l;
  guint n_rows = 0;
  GList *info;
  const char **field_names = empathy_contact_info_get_field_names (NULL);
  guint i;

  g_assert (self->priv->details_to_set == NULL);

  self->priv->details_changed = FALSE;

  contact = empathy_contact_get_tp_contact (self->priv->contact);
  connection = tp_contact_get_connection (contact);

  info = tp_contact_get_contact_info (contact);

  specs = tp_connection_get_contact_info_supported_fields (connection);

  /* Look at the fields set in our vCard */
  for (l = info; l != NULL; l = l->next)
    {
      TpContactInfoField *field = l->data;

      /* make a copy for the details_to_set list */
      field = tp_contact_info_field_copy (field);
      DEBUG ("Field %s is in our vCard", field->field_name);

      self->priv->details_to_set = g_list_prepend (self->priv->details_to_set,
          field);
    }

  /* Add fields which are supported but not in the vCard */
  for (i = 0; field_names[i] != NULL; i++)
    {
      TpContactInfoFieldSpec *spec;
      TpContactInfoField *field;

      /* Check if the field was in the vCard */
      if (field_name_in_field_list (self->priv->details_to_set,
            field_names[i]))
        continue;

      /* Check if the CM supports the field */
      spec = get_spec_from_list (specs, field_names[i]);
      if (spec == NULL)
        continue;

      /* add an empty field so user can set a value */
      field = tp_contact_info_field_new (spec->name, spec->parameters, NULL);

      self->priv->details_to_set = g_list_prepend (self->priv->details_to_set,
          field);
    }

  /* Add widgets for supported fields */
  self->priv->details_to_set = g_list_sort (self->priv->details_to_set,
      (GCompareFunc) empathy_contact_info_field_spec_cmp);

  for (l = self->priv->details_to_set; l != NULL; l= g_list_next (l))
    {
      TpContactInfoField *field = l->data;
      GtkWidget *label, *w;
      TpContactInfoFieldSpec *spec;
      gboolean has_field;
      char *title;

      has_field = empathy_contact_info_lookup_field (field->field_name,
          NULL, NULL);
      if (!has_field)
        {
          /* Empathy doesn't display this field so we can't change it.
           * But we put it in the details_to_set list so it won't be erased
           * when calling SetContactInfo (bgo #630427) */
          DEBUG ("Unhandled ContactInfo field spec: %s", field->field_name);
          continue;
        }

      spec = get_spec_from_list (specs, field->field_name);
      /* We shouldn't have added the field to details_to_set if it's not
       * supported by the CM */
      g_assert (spec != NULL);

      if (spec->flags & TP_CONTACT_INFO_FIELD_FLAG_OVERWRITTEN_BY_NICKNAME)
        {
          DEBUG ("Ignoring field '%s' due it to having the "
              "Overwritten_By_Nickname flag", field->field_name);
          continue;
        }

      /* Add Title */
      title = empathy_contact_info_field_label (field->field_name,
          field->parameters,
          (spec->flags & TP_CONTACT_INFO_FIELD_FLAG_PARAMETERS_EXACT));
      label = gtk_label_new (title);
      g_free (title);

      /* TODO: if TP_CONTACT_INFO_FIELD_FLAG_PARAMETERS_EXACT is not set we
       * should allow user to tag the vCard fields (bgo#672034) */

      /* Add Value */
      if (!tp_strdiff (field->field_name, "bday"))
        {
          w = empathy_calendar_button_new ();

          if (field->field_value[0])
            {
              GDate date;

              g_date_set_parse (&date, field->field_value[0]);
              if (g_date_valid (&date))
                {
                  empathy_calendar_button_set_date (EMPATHY_CALENDAR_BUTTON (w),
                      &date);
                }
            }

          g_signal_connect (w, "date-changed",
            G_CALLBACK (contact_widget_bday_changed_cb), self);
        }
      else
        {
          w = gtk_entry_new ();
          gtk_entry_set_text (GTK_ENTRY (w),
              field->field_value[0] ? field->field_value[0] : "");
          g_signal_connect (w, "changed",
            G_CALLBACK (contact_widget_details_changed_cb), self);
        }

      gtk_widget_show_all (w);
      add_row (GTK_GRID (self->priv->grid_contact), label, w);

      g_object_set_data ((GObject *) w, DATA_FIELD, field);

      n_rows++;
    }

  g_list_free (specs);
  g_list_free (info);

  return n_rows;
}

static guint
contact_widget_details_update_show (EmpathyContactWidget *self)
{
  TpContact *contact;
  GList *info, *l;
  guint n_rows = 0;
  GtkWidget *channels_label;
  TpAccount *account;

  contact = empathy_contact_get_tp_contact (self->priv->contact);
  info = tp_contact_get_contact_info (contact);
  info = g_list_sort (info, (GCompareFunc) empathy_contact_info_field_cmp);
  for (l = info; l != NULL; l = l->next)
    {
      TpContactInfoField *field = l->data;
      const gchar *value;
      gchar *markup = NULL, *title;
      GtkWidget *title_widget, *value_widget;
      EmpathyContactInfoFormatFunc format;

      if (field->field_value == NULL || field->field_value[0] == NULL)
        continue;

      value = field->field_value[0];

      if (!empathy_contact_info_lookup_field (field->field_name, NULL, &format))
        {
          DEBUG ("Unhandled ContactInfo field: %s", field->field_name);
          continue;
        }

      if (format != NULL)
        {
          markup = format (field->field_value);

          if (markup == NULL)
            {
              DEBUG ("Invalid value for field '%s' (first element was '%s')",
                  field->field_name, field->field_value[0]);
              continue;
            }
        }

      /* Add Title */
      title = empathy_contact_info_field_label (field->field_name,
          field->parameters, TRUE);
      title_widget = gtk_label_new (title);
      g_free (title);

      /* Add Value */
      value_widget = gtk_label_new (value);
      if (markup != NULL)
        {
          gtk_label_set_markup (GTK_LABEL (value_widget), markup);
          g_free (markup);
        }

      if ((self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP) == 0)
        gtk_label_set_selectable (GTK_LABEL (value_widget), TRUE);

      add_row (GTK_GRID (self->priv->grid_contact), title_widget,
          value_widget);

      n_rows++;
    }

  account = empathy_contact_get_account (self->priv->contact);

  channels_label = empathy_contact_info_create_channel_list_label (account,
      info, n_rows);

  if (channels_label != NULL)
    {
      GtkWidget *title_widget;

      title_widget =  gtk_label_new (_("Channels:"));

      add_row (GTK_GRID (self->priv->grid_contact), title_widget,
          channels_label);

      n_rows++;
    }

  g_list_free (info);

  return n_rows;
}

static void
contact_widget_foreach (GtkWidget *widget,
    gpointer data)
{
  if (g_object_get_data (G_OBJECT (widget), "added-row") != NULL)
    gtk_widget_destroy (widget);
}

static void
contact_widget_details_notify_cb (EmpathyContactWidget *self)
{
  guint n_rows;

  gtk_container_foreach (GTK_CONTAINER (self->priv->grid_contact),
      contact_widget_foreach, NULL);

  if ((self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_DETAILS) != 0)
    n_rows = contact_widget_details_update_edit (self);
  else
    n_rows = contact_widget_details_update_show (self);

  gtk_widget_set_visible (self->priv->label_details, n_rows > 0);

  gtk_widget_hide (self->priv->hbox_details_requested);
  gtk_spinner_stop (GTK_SPINNER (self->priv->spinner_details));
}

static void
contact_widget_details_request_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpContact *contact = TP_CONTACT (object);
  EmpathyContactWidget *self = user_data;
  GError *error = NULL;

  if (!tp_contact_request_contact_info_finish (contact, res, &error))
    {
      /* If the request got cancelled it could mean the contact widget is
       * destroyed, so we should not dereference self */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          return;
        }

      gtk_widget_hide (self->priv->label_details);
      g_clear_error (&error);
    }
  else
    {
      contact_widget_details_notify_cb (self);
    }

  /* If we are going to edit ContactInfo, we don't want live updates */
  if ((self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_DETAILS) == 0)
    {
      g_signal_connect_swapped (contact, "notify::contact-info",
          G_CALLBACK (contact_widget_details_notify_cb), self);
    }

  tp_clear_object (&self->priv->details_cancellable);
}

static void
fetch_contact_information (EmpathyContactWidget *self,
    TpConnection *connection)
{
  TpContact *contact;
  TpContactInfoFlags flags;

  if (!tp_proxy_has_interface_by_id (connection,
          TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACT_INFO))
    {
      gtk_widget_hide (self->priv->label_details);
      return;
    }

  /* If we want to edit info, but connection does not support that, stop */
  flags = tp_connection_get_contact_info_flags (connection);
  if ((flags & TP_CONTACT_INFO_FLAG_CAN_SET) == 0 &&
      (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_DETAILS) != 0)
    {
      gtk_widget_hide (self->priv->label_details);
      return;
    }

  /* Request the contact's info */
  gtk_widget_show (self->priv->hbox_details_requested);
  gtk_spinner_start (GTK_SPINNER (self->priv->spinner_details));

  contact = empathy_contact_get_tp_contact (self->priv->contact);
  g_assert (self->priv->details_cancellable == NULL);
  self->priv->details_cancellable = g_cancellable_new ();
  tp_contact_request_contact_info_async (contact,
      self->priv->details_cancellable, contact_widget_details_request_cb,
      self);
}

static void
contact_widget_details_update (EmpathyContactWidget *self)
{
  TpContact *tp_contact = NULL;

  if ((self->priv->flags & EMPATHY_CONTACT_WIDGET_SHOW_DETAILS) == 0 &&
      (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_DETAILS) == 0)
    return;

  gtk_widget_hide (self->priv->label_details);

  if (self->priv->contact != NULL)
    tp_contact = empathy_contact_get_tp_contact (self->priv->contact);

  if (tp_contact != NULL)
    {
      TpConnection *connection;

      connection = tp_contact_get_connection (tp_contact);

      fetch_contact_information (self, connection);
    }
}

static void
contact_widget_client_update (EmpathyContactWidget *self)
{
  /* FIXME: Needs new telepathy spec */
}

static void
contact_widget_client_setup (EmpathyContactWidget *self)
{
  /* FIXME: Needs new telepathy spec */
  gtk_widget_hide (self->priv->vbox_client);
}

static void
contact_widget_groups_update (EmpathyContactWidget *self)
{
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_GROUPS &&
      self->priv->contact != NULL)
    {
      FolksPersona *persona =
          empathy_contact_get_persona (self->priv->contact);

      if (FOLKS_IS_GROUP_DETAILS (persona))
        {
          empathy_groups_widget_set_group_details (
              EMPATHY_GROUPS_WIDGET (self->priv->groups_widget),
              FOLKS_GROUP_DETAILS (persona));
          gtk_widget_show (self->priv->groups_widget);

          return;
        }
    }

  /* In case of failure */
  gtk_widget_hide (self->priv->groups_widget);
}

/* Converts the Location's GHashTable's key to a user readable string */
static const gchar *
location_key_to_label (const gchar *key)
{
  if (tp_strdiff (key, EMPATHY_LOCATION_COUNTRY_CODE) == FALSE)
    return _("Country ISO Code:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_COUNTRY) == FALSE)
    return _("Country:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_REGION) == FALSE)
    return _("State:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_LOCALITY) == FALSE)
    return _("City:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_AREA) == FALSE)
    return _("Area:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_POSTAL_CODE) == FALSE)
    return _("Postal Code:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_STREET) == FALSE)
    return _("Street:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_BUILDING) == FALSE)
    return _("Building:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_FLOOR) == FALSE)
    return _("Floor:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_ROOM) == FALSE)
    return _("Room:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_TEXT) == FALSE)
    return _("Text:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_DESCRIPTION) == FALSE)
    return _("Description:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_URI) == FALSE)
    return _("URI:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_ACCURACY_LEVEL) == FALSE)
    return _("Accuracy Level:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_ERROR) == FALSE)
    return _("Error:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_VERTICAL_ERROR_M) == FALSE)
    return _("Vertical Error (meters):");
  else if (tp_strdiff (key, EMPATHY_LOCATION_HORIZONTAL_ERROR_M) == FALSE)
    return _("Horizontal Error (meters):");
  else if (tp_strdiff (key, EMPATHY_LOCATION_SPEED) == FALSE)
    return _("Speed:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_BEARING) == FALSE)
    return _("Bearing:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_CLIMB) == FALSE)
    return _("Climb Speed:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_TIMESTAMP) == FALSE)
    return _("Last Updated on:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_LON) == FALSE)
    return _("Longitude:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_LAT) == FALSE)
    return _("Latitude:");
  else if (tp_strdiff (key, EMPATHY_LOCATION_ALT) == FALSE)
    return _("Altitude:");
  else
  {
    DEBUG ("Unexpected Location key: %s", key);
    return key;
  }
}

static void
contact_widget_location_update (EmpathyContactWidget *self)
{
  GHashTable *location;
  GValue *value;
#ifdef HAVE_LIBCHAMPLAIN
  gdouble lat = 0.0, lon = 0.0;
  gboolean has_position = TRUE;
#endif
  GtkWidget *label;
  guint row = 0;
  static const gchar* ordered_geolocation_keys[] = {
    EMPATHY_LOCATION_TEXT,
    EMPATHY_LOCATION_URI,
    EMPATHY_LOCATION_DESCRIPTION,
    EMPATHY_LOCATION_BUILDING,
    EMPATHY_LOCATION_FLOOR,
    EMPATHY_LOCATION_ROOM,
    EMPATHY_LOCATION_STREET,
    EMPATHY_LOCATION_AREA,
    EMPATHY_LOCATION_LOCALITY,
    EMPATHY_LOCATION_REGION,
    EMPATHY_LOCATION_COUNTRY,
    NULL
  };
  int i;
  const gchar *skey;
  gboolean display_map = FALSE;

  if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_SHOW_LOCATION))
    {
      gtk_widget_hide (self->priv->vbox_location);
      return;
    }

  location = empathy_contact_get_location (self->priv->contact);
  if (location == NULL || g_hash_table_size (location) == 0)
    {
      gtk_widget_hide (self->priv->vbox_location);
      return;
    }

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_TIMESTAMP);
  if (value == NULL)
    {
      gchar *loc = g_strdup_printf ("<b>%s</b>", _("Location"));
      gtk_label_set_markup (GTK_LABEL (self->priv->label_location), loc);
      g_free (loc);
    }
  else
    {
      gchar *user_date;
      gchar *text;
      gint64 stamp;
      gchar *tmp;

      stamp = g_value_get_int64 (value);

      user_date = empathy_time_to_string_relative (stamp);

      tmp = g_strdup_printf ("<b>%s</b>", _("Location"));
      /* translators: format is "Location, $date" */
      text = g_strdup_printf (_("%s, %s"), tmp, user_date);
      g_free (tmp);
      gtk_label_set_markup (GTK_LABEL (self->priv->label_location), text);
      g_free (user_date);
      g_free (text);
    }


  /* Prepare the location self grid */
  if (self->priv->grid_location != NULL)
    {
      gtk_widget_destroy (self->priv->grid_location);
    }

  self->priv->grid_location = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (self->priv->subvbox_location),
      self->priv->grid_location, FALSE, FALSE, 5);


  for (i = 0; (skey = ordered_geolocation_keys[i]); i++)
    {
      const gchar* user_label;
      GValue *gvalue;
      char *svalue = NULL;

      gvalue = g_hash_table_lookup (location, (gpointer) skey);
      if (gvalue == NULL)
        continue;

      user_label = location_key_to_label (skey);

      label = gtk_label_new (user_label);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_grid_attach (GTK_GRID (self->priv->grid_location),
          label, 0, row, 1, 1);
      gtk_widget_show (label);

      if (G_VALUE_TYPE (gvalue) == G_TYPE_DOUBLE)
        {
          gdouble dvalue;
          dvalue = g_value_get_double (gvalue);
          svalue = g_strdup_printf ("%f", dvalue);
        }
      else if (G_VALUE_TYPE (gvalue) == G_TYPE_STRING)
        {
          svalue = g_value_dup_string (gvalue);
        }
      else if (G_VALUE_TYPE (gvalue) == G_TYPE_INT64)
        {
          gint64 time_;

          time_ = g_value_get_int64 (value);
          svalue = empathy_time_to_string_utc (time_, _("%B %e, %Y at %R UTC"));
        }

      if (svalue != NULL)
        {
          label = gtk_label_new (svalue);
          gtk_grid_attach (GTK_GRID (self->priv->grid_location),
              label, 1, row, 1, 1);
          gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
          gtk_widget_show (label);

          if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP))
            gtk_label_set_selectable (GTK_LABEL (label), TRUE);
        }

      g_free (svalue);
      row++;
    }

#ifdef HAVE_LIBCHAMPLAIN
  if (has_position &&
      !(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP))
    {
      /* Cannot be displayed in tooltips until Clutter-Gtk can deal with such
       * windows */
      display_map = TRUE;
    }
#endif

  if (row > 0)
    {
      /* We can display some fields */
      gtk_widget_show (self->priv->grid_location);
    }
  else if (!display_map)
    {
      /* Can't display either fields or map */
      gtk_widget_hide (self->priv->vbox_location);
      return;
    }

#ifdef HAVE_LIBCHAMPLAIN
  if (display_map)
    {
      ClutterActor *marker;
      ChamplainMarkerLayer *layer;

      self->priv->map_view_embed = gtk_champlain_embed_new ();
      self->priv->map_view = gtk_champlain_embed_get_view (
          GTK_CHAMPLAIN_EMBED (self->priv->map_view_embed));

      gtk_container_add (GTK_CONTAINER (self->priv->viewport_map),
          self->priv->map_view_embed);
      g_object_set (G_OBJECT (self->priv->map_view),
          "kinetic-mode", TRUE,
          "zoom-level", 10,
          NULL);

      layer = champlain_marker_layer_new ();
      champlain_view_add_layer (self->priv->map_view, CHAMPLAIN_LAYER (layer));

      marker = champlain_label_new_with_text (
          empathy_contact_get_alias (self->priv->contact), NULL, NULL, NULL);
      champlain_location_set_location (CHAMPLAIN_LOCATION (marker), lat, lon);
      champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));

      champlain_view_center_on (self->priv->map_view, lat, lon);
      gtk_widget_show_all (self->priv->viewport_map);
    }
#endif

    gtk_widget_show (self->priv->vbox_location);
}

static void
save_avatar_menu_activate_cb (GtkWidget *widget,
    EmpathyContactWidget *self)
{
  GtkWidget *dialog;
  EmpathyAvatar *avatar;
  gchar *ext = NULL, *filename;

  dialog = gtk_file_chooser_dialog_new (_("Save Avatar"),
      NULL,
      GTK_FILE_CHOOSER_ACTION_SAVE,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
      NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog),
      TRUE);

  /* look for the avatar extension */
  avatar = empathy_contact_get_avatar (self->priv->contact);
  if (avatar->format != NULL)
    {
      gchar **splitted;

      splitted = g_strsplit (avatar->format, "/", 2);
      if (splitted[0] != NULL && splitted[1] != NULL)
          ext = g_strdup (splitted[1]);

      g_strfreev (splitted);
    }
  else
    {
      /* Avatar was loaded from the cache so was converted to PNG */
      ext = g_strdup ("png");
    }

  if (ext != NULL)
    {
      gchar *id;

      id = tp_escape_as_identifier (empathy_contact_get_id (
            self->priv->contact));

      filename = g_strdup_printf ("%s.%s", id, ext);
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);

      g_free (id);
      g_free (ext);
      g_free (filename);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      GError *error = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (!empathy_avatar_save_to_file (avatar, filename, &error))
        {
          /* Save error */
          GtkWidget *error_dialog;

          error_dialog = gtk_message_dialog_new (NULL, 0,
              GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
              _("Unable to save avatar"));

          gtk_message_dialog_format_secondary_text (
              GTK_MESSAGE_DIALOG (error_dialog), "%s", error->message);

          g_signal_connect (error_dialog, "response",
              G_CALLBACK (gtk_widget_destroy), NULL);

          gtk_window_present (GTK_WINDOW (error_dialog));

          g_clear_error (&error);
        }

      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

static void
popup_avatar_menu (EmpathyContactWidget *self,
                   GtkWidget *parent,
                   GdkEventButton *event)
{
  GtkWidget *menu, *item;
  gint button, event_time;

  if (self->priv->contact == NULL ||
      empathy_contact_get_avatar (self->priv->contact) == NULL)
      return;

  menu = empathy_context_menu_new (parent);

  /* Add "Save as..." entry */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
      G_CALLBACK (save_avatar_menu_activate_cb), self);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
      button, event_time);
}

static gboolean
widget_avatar_popup_menu_cb (GtkWidget *widget,
                             EmpathyContactWidget *self)
{
  popup_avatar_menu (self, widget, NULL);

  return TRUE;
}

static gboolean
widget_avatar_button_press_event_cb (GtkWidget *widget,
                                     GdkEventButton *event,
                                     EmpathyContactWidget *self)
{
  /* Ignore double-clicks and triple-clicks */
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      popup_avatar_menu (self, widget, event);
      return TRUE;
    }

  return FALSE;
}

static void
set_nickname_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;

  if (!tp_account_set_nickname_finish (TP_ACCOUNT (source), res, &error))
    {
      DEBUG ("Failed to set Account.Nickname: %s", error->message);
      g_error_free (error);
    }
}

/* Update all the contact info fields having the
* Overwritten_By_Nickname flag to the new alias. This avoid accidentally
* reseting the alias when calling SetContactInfo(). See bgo #644298 for
* details. */
static void
update_nickname_in_contact_info (EmpathyContactWidget *self,
    TpConnection *connection,
    const gchar *nickname)
{
  GList *specs, *l;

  specs = tp_connection_get_contact_info_supported_fields (connection);

  for (l = self->priv->details_to_set; l != NULL; l= g_list_next (l))
    {
      TpContactInfoField *field = l->data;
      TpContactInfoFieldSpec *spec;

      spec = get_spec_from_list (specs, field->field_name);
      /* We shouldn't have added the field to details_to_set if it's not
       * supported by the CM */
      g_assert (spec != NULL);

      if (spec->flags & TP_CONTACT_INFO_FIELD_FLAG_OVERWRITTEN_BY_NICKNAME)
        {
          const gchar *strv[] = { nickname, NULL };

          DEBUG ("Updating field '%s' to '%s' as it has the "
              "Overwritten_By_Nickname flag and Account.Nickname has "
              "been updated", field->field_name, nickname);

          if (field->field_value != NULL)
            g_strfreev (field->field_value);
          field->field_value = g_strdupv ((GStrv) strv);
        }
    }

  g_list_free (specs);
}

static gboolean
contact_widget_entry_alias_focus_event_cb (GtkEditable *editable,
    GdkEventFocus *event,
    EmpathyContactWidget *self)
{
  if (self->priv->contact)
    {
      const gchar *alias;

      alias = gtk_entry_get_text (GTK_ENTRY (editable));

      if (empathy_contact_is_user (self->priv->contact))
        {
          TpAccount * account;
          const gchar *current_nickname;

          account = empathy_contact_get_account (self->priv->contact);
          current_nickname = tp_account_get_nickname (account);

          if (tp_strdiff (current_nickname, alias))
            {
              DEBUG ("Set Account.Nickname to %s", alias);

              tp_account_set_nickname_async (account, alias, set_nickname_cb,
                  NULL);

              update_nickname_in_contact_info (self,
                  empathy_contact_get_connection (self->priv->contact), alias);
            }
        }
      else
        {
          empathy_contact_set_alias (self->priv->contact, alias);
        }
    }

  return FALSE;
}

static void
contact_widget_name_notify_cb (EmpathyContactWidget *self)
{
  if (GTK_IS_ENTRY (self->priv->widget_alias))
      gtk_entry_set_text (GTK_ENTRY (self->priv->widget_alias),
          empathy_contact_get_alias (self->priv->contact));
  else
      gtk_label_set_label (GTK_LABEL (self->priv->widget_alias),
          empathy_contact_get_alias (self->priv->contact));
}

static void
contact_widget_presence_notify_cb (EmpathyContactWidget *self)
{
  const gchar *status;
  gchar *markup_text = NULL;

  status = empathy_contact_get_status (self->priv->contact);
  if (status != NULL)
    markup_text = empathy_add_link_markup (status);
  gtk_label_set_markup (GTK_LABEL (self->priv->label_status), markup_text);
  g_free (markup_text);

  gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->image_state),
      empathy_icon_name_for_contact (self->priv->contact),
      GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (self->priv->image_state);
}

static void
contact_widget_remove_contact (EmpathyContactWidget *self)
{
  if (self->priv->contact)
    {
      TpContact *tp_contact;

      contact_widget_save (self);

      g_signal_handlers_disconnect_by_func (self->priv->contact,
          contact_widget_name_notify_cb, self);
      g_signal_handlers_disconnect_by_func (self->priv->contact,
          contact_widget_presence_notify_cb, self);

      tp_contact = empathy_contact_get_tp_contact (self->priv->contact);
      if (tp_contact != NULL)
        {
          g_signal_handlers_disconnect_by_func (tp_contact,
              contact_widget_details_notify_cb, self);
        }

      g_object_unref (self->priv->contact);
      self->priv->contact = NULL;
    }

  if (self->priv->details_cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->details_cancellable);
      tp_clear_object (&self->priv->details_cancellable);
    }
}

static void contact_widget_change_contact (EmpathyContactWidget *self);

static void
contact_widget_contact_update (EmpathyContactWidget *self)
{
  TpAccount *account = NULL;
  const gchar *id = NULL;

  /* Connect and get info from new contact */
  if (self->priv->contact)
    {
      g_signal_connect_swapped (self->priv->contact, "notify::name",
          G_CALLBACK (contact_widget_name_notify_cb), self);
      g_signal_connect_swapped (self->priv->contact, "notify::presence",
          G_CALLBACK (contact_widget_presence_notify_cb), self);
      g_signal_connect_swapped (self->priv->contact,
          "notify::presence-message",
          G_CALLBACK (contact_widget_presence_notify_cb), self);

      account = empathy_contact_get_account (self->priv->contact);
      id = empathy_contact_get_id (self->priv->contact);
    }

  /* Update account widget */
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT)
    {
      if (account)
        {
          g_signal_handlers_block_by_func (self->priv->widget_account,
                   contact_widget_change_contact,
                   self);
          empathy_account_chooser_set_account (
              EMPATHY_ACCOUNT_CHOOSER (self->priv->widget_account), account);
          g_signal_handlers_unblock_by_func (self->priv->widget_account,
              contact_widget_change_contact, self);
        }
    }
  else
    {
      if ((self->priv->flags & EMPATHY_CONTACT_WIDGET_NO_ACCOUNT) == 0)
        {
          if (account)
            {
              const gchar *name;

              name = tp_account_get_display_name (account);
              gtk_label_set_label (GTK_LABEL (self->priv->label_account),
                  name);

              name = tp_account_get_icon_name (account);
              gtk_image_set_from_icon_name (
                  GTK_IMAGE (self->priv->image_account),
                  name, GTK_ICON_SIZE_MENU);
            }
        }
    }

  /* Update id widget */
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
      gtk_entry_set_text (GTK_ENTRY (self->priv->widget_id), id ? id : "");
  else
      gtk_label_set_label (GTK_LABEL (self->priv->widget_id), id ? id : "");

  /* Update other widgets */
  if (self->priv->contact)
    {
      contact_widget_name_notify_cb (self);
      contact_widget_presence_notify_cb (self);

      gtk_widget_show (self->priv->label_alias);
      gtk_widget_show (self->priv->widget_alias);
      gtk_widget_show (self->priv->widget_avatar);

      gtk_widget_set_visible (self->priv->hbox_presence,
          !(self->priv->flags & EMPATHY_CONTACT_WIDGET_NO_STATUS));

      if (empathy_contact_is_user (self->priv->contact))
        gtk_label_set_text (GTK_LABEL (self->priv->label_details),
            _("Personal Details"));
      else
        gtk_label_set_text (GTK_LABEL (self->priv->label_details),
            _("Contact Details"));
    }
  else
    {
      gtk_widget_hide (self->priv->label_alias);
      gtk_widget_hide (self->priv->widget_alias);
      gtk_widget_hide (self->priv->hbox_presence);
      gtk_widget_hide (self->priv->widget_avatar);
    }
}

static void
contact_widget_set_contact (EmpathyContactWidget *self,
                            EmpathyContact *contact)
{
  if (contact == self->priv->contact)
    return;

  contact_widget_remove_contact (self);
  if (contact)
    self->priv->contact = g_object_ref (contact);

  /* set the selected account to be the account this contact came from */
  if (contact && EMPATHY_IS_ACCOUNT_CHOOSER (self->priv->widget_account)) {
      empathy_account_chooser_set_account (
		      EMPATHY_ACCOUNT_CHOOSER (self->priv->widget_account),
		      empathy_contact_get_account (contact));
  }

  /* Update self for widgets */
  contact_widget_contact_update (self);
  contact_widget_groups_update (self);
  contact_widget_details_update (self);
  contact_widget_client_update (self);
  contact_widget_location_update (self);
}

static void
contact_widget_got_contact_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  EmpathyContactWidget *self = user_data;
  GError *error = NULL;
  EmpathyContact *contact;

  contact = empathy_client_factory_dup_contact_by_id_finish (
      EMPATHY_CLIENT_FACTORY (source), result, &error);

  if (contact == NULL)
    {
      DEBUG ("Error: %s", error->message);
      g_error_free (error);
      goto out;
    }

  contact_widget_set_contact (self, contact);

  g_object_unref (contact);
out:
  g_object_unref (self);
}

static void
contact_widget_change_contact (EmpathyContactWidget *self)
{
  TpConnection *connection;

  connection = empathy_account_chooser_get_connection (
      EMPATHY_ACCOUNT_CHOOSER (self->priv->widget_account));
  if (!connection)
      return;

  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    {
      const gchar *id;

      id = gtk_entry_get_text (GTK_ENTRY (self->priv->widget_id));
      if (!EMP_STR_EMPTY (id))
        {
          EmpathyClientFactory *factory;

          factory = empathy_client_factory_dup ();

          empathy_client_factory_dup_contact_by_id_async (factory, connection,
              id, contact_widget_got_contact_cb, g_object_ref (self));

          g_object_unref (factory);
        }
    }
  else
    {
      EmpathyContact *contact;

      contact = empathy_contact_dup_from_tp_contact (
          tp_connection_get_self_contact (connection));

      contact_widget_set_contact (self, contact);
      g_object_unref (contact);
    }
}

static gboolean
contact_widget_id_activate_timeout (EmpathyContactWidget *self)
{
  contact_widget_change_contact (self);
  return FALSE;
}

static void
contact_widget_id_changed_cb (GtkEntry *entry,
                              EmpathyContactWidget *self)
{
  if (self->priv->widget_id_timeout != 0)
    {
      g_source_remove (self->priv->widget_id_timeout);
    }

  self->priv->widget_id_timeout =
    g_timeout_add_seconds (ID_CHANGED_TIMEOUT,
        (GSourceFunc) contact_widget_id_activate_timeout, self);
}

static gboolean
contact_widget_id_focus_out_cb (GtkWidget *widget,
                                GdkEventFocus *event,
                                EmpathyContactWidget *self)
{
  contact_widget_change_contact (self);
  return FALSE;
}

static void
contact_widget_contact_setup (EmpathyContactWidget *self)
{
  self->priv->label_status = gtk_label_new ("");
  gtk_label_set_line_wrap_mode (GTK_LABEL (self->priv->label_status),
                                PANGO_WRAP_WORD_CHAR);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->label_status),
                           TRUE);
  gtk_misc_set_alignment (GTK_MISC (self->priv->label_status), 0, 0.5);

  if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP))
    gtk_label_set_selectable (GTK_LABEL (self->priv->label_status), TRUE);

  gtk_box_pack_start (GTK_BOX (self->priv->hbox_presence),
        self->priv->label_status, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->label_status);

  /* Setup account label/chooser */
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT)
    {
      self->priv->widget_account = empathy_account_chooser_new ();

      g_signal_connect_swapped (self->priv->widget_account, "changed",
            G_CALLBACK (contact_widget_change_contact),
            self);
    }
  else if (self->priv->flags & EMPATHY_CONTACT_WIDGET_NO_ACCOUNT)
    {
      /* Don't display the account */
      gtk_widget_hide (self->priv->label_left_account);
    }
  else
    {
      /* Pack the protocol icon with the account name in an hbox */
      self->priv->widget_account = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

      self->priv->label_account = gtk_label_new (NULL);
      if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (self->priv->label_account), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (self->priv->label_account), 0, 0.5);
      gtk_widget_show (self->priv->label_account);

      self->priv->image_account = gtk_image_new ();
      gtk_widget_show (self->priv->image_account);

      gtk_box_pack_start (GTK_BOX (self->priv->widget_account),
          self->priv->image_account, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (self->priv->widget_account),
          self->priv->label_account, FALSE, TRUE, 0);
    }

  if (self->priv->widget_account != NULL)
    {
      gtk_grid_attach (GTK_GRID (self->priv->grid_contact),
          self->priv->widget_account,
          2, 0, 1, 1);

      gtk_widget_show (self->priv->widget_account);
    }

  /* Set up avatar display */
    {
      self->priv->widget_avatar = empathy_avatar_image_new ();

      g_signal_connect (self->priv->widget_avatar, "popup-menu",
          G_CALLBACK (widget_avatar_popup_menu_cb), self);
      g_signal_connect (self->priv->widget_avatar, "button-press-event",
          G_CALLBACK (widget_avatar_button_press_event_cb), self);
    }

  gtk_box_pack_start (GTK_BOX (self->priv->vbox_avatar),
          self->priv->widget_avatar,
          FALSE, FALSE,
          6);
  gtk_widget_show (self->priv->widget_avatar);

  /* Setup id label/entry */
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    {
      self->priv->widget_id = gtk_entry_new ();
      g_signal_connect (self->priv->widget_id, "focus-out-event",
            G_CALLBACK (contact_widget_id_focus_out_cb),
            self);
      g_signal_connect (self->priv->widget_id, "changed",
            G_CALLBACK (contact_widget_id_changed_cb),
            self);
    }
  else
    {
      self->priv->widget_id = gtk_label_new (NULL);
      if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (self->priv->widget_id), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (self->priv->widget_id), 0, 0.5);
    }

  gtk_grid_attach (GTK_GRID (self->priv->grid_contact), self->priv->widget_id,
      2, 1, 1, 1);
  gtk_widget_set_hexpand (self->priv->widget_id, TRUE);

  gtk_widget_show (self->priv->widget_id);

  /* Setup alias label/entry */
  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ALIAS)
    {
      self->priv->widget_alias = gtk_entry_new ();

      if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_NO_SET_ALIAS))
        g_signal_connect (self->priv->widget_alias, "focus-out-event",
              G_CALLBACK (contact_widget_entry_alias_focus_event_cb),
              self);

      /* Make return activate the window default (the Close button) */
      gtk_entry_set_activates_default (GTK_ENTRY (self->priv->widget_alias),
          TRUE);
    }
  else
    {
      self->priv->widget_alias = gtk_label_new (NULL);
      if (!(self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (self->priv->widget_alias), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (self->priv->widget_alias), 0, 0.5);
    }

  gtk_grid_attach (GTK_GRID (self->priv->grid_contact),
      self->priv->widget_alias, 2, 2, 1, 1);
  gtk_widget_set_hexpand (self->priv->widget_alias, TRUE);

  if (self->priv->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP) {
    gtk_label_set_selectable (GTK_LABEL (self->priv->label_status), FALSE);
  }
  gtk_widget_show (self->priv->widget_alias);
}

static void
empathy_contact_widget_finalize (GObject *object)
{
  EmpathyContactWidget *self = EMPATHY_CONTACT_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_contact_widget_parent_class)->finalize;

  contact_widget_remove_contact (self);

  if (self->priv->widget_id_timeout != 0)
    {
      g_source_remove (self->priv->widget_id_timeout);
    }


  if (chain_up != NULL)
    chain_up (object);
}

/**
 * empathy_contact_widget_new:
 * @contact: an #EmpathyContact
 * @flags: #EmpathyContactWidgetFlags for the new contact widget
 *
 * Creates a new #EmpathyContactWidget.
 *
 * Return value: a new #EmpathyContactWidget
 */
GtkWidget *
empathy_contact_widget_new (EmpathyContact *contact,
                            EmpathyContactWidgetFlags flags)
{
  EmpathyContactWidget *self;
  gchar *filename;
  GtkWidget *main_vbox;
  GtkBuilder *gui;

  g_return_val_if_fail (contact == NULL || EMPATHY_IS_CONTACT (contact), NULL);

  self = g_object_new (EMPATHY_TYPE_CONTACT_WIDGET, NULL);

  self->priv->flags = flags;

  filename = empathy_file_lookup ("empathy-contact-widget.ui",
      "libempathy-gtk");
  gui = empathy_builder_get_file (filename,
       "vbox_contact_widget", &main_vbox,
       "hbox_presence", &self->priv->hbox_presence,
       "label_alias", &self->priv->label_alias,
       "image_state", &self->priv->image_state,
       "grid_contact", &self->priv->grid_contact,
       "vbox_avatar", &self->priv->vbox_avatar,
       "vbox_location", &self->priv->vbox_location,
       "subvbox_location", &self->priv->subvbox_location,
       "label_location", &self->priv->label_location,
#ifdef HAVE_LIBCHAMPLAIN
       "viewport_map", &self->priv->viewport_map,
#endif
       "groups_widget", &self->priv->groups_widget,
       "hbox_details_requested", &self->priv->hbox_details_requested,
       "vbox_client", &self->priv->vbox_client,
       "grid_client", &self->priv->grid_client,
       "hbox_client_requested", &self->priv->hbox_client_requested,
       "label_details", &self->priv->label_details,
       "label_left_account", &self->priv->label_left_account,
       NULL);
  g_free (filename);

  gtk_container_add (GTK_CONTAINER (self), main_vbox);
  gtk_widget_show (GTK_WIDGET (main_vbox));

  /* Create widgets */
  contact_widget_contact_setup (self);
  contact_widget_details_setup (self);
  contact_widget_client_setup (self);

  if (contact != NULL)
    contact_widget_set_contact (self, contact);
  else if (self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT ||
      self->priv->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    contact_widget_change_contact (self);

  g_object_unref (gui);

  return GTK_WIDGET (self);
}

/**
 * empathy_contact_widget_get_contact:
 * @widget: an #EmpathyContactWidget
 *
 * Get the #EmpathyContact related with the #EmpathyContactWidget @widget.
 *
 * Returns: the #EmpathyContact associated with @widget
 */
EmpathyContact *
empathy_contact_widget_get_contact (GtkWidget *widget)
{
  EmpathyContactWidget *self = EMPATHY_CONTACT_WIDGET (widget);

  return self->priv->contact;
}

const gchar *
empathy_contact_widget_get_alias (GtkWidget *widget)
{
  EmpathyContactWidget *self = EMPATHY_CONTACT_WIDGET (widget);

  return gtk_entry_get_text (GTK_ENTRY (self->priv->widget_alias));
}

/**
 * empathy_contact_widget_set_contact:
 * @widget: an #EmpathyContactWidget
 * @contact: a different #EmpathyContact
 *
 * Change the #EmpathyContact related with the #EmpathyContactWidget @widget.
 */
void
empathy_contact_widget_set_contact (GtkWidget *widget,
                                    EmpathyContact *contact)
{
  EmpathyContactWidget *self = EMPATHY_CONTACT_WIDGET (widget);

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  contact_widget_set_contact (self, contact);
}

/**
 * empathy_contact_widget_set_account_filter:
 * @widget: an #EmpathyContactWidget
 * @filter: a #EmpathyAccountChooserFilterFunc
 * @user_data: user data to pass to @filter, or %NULL
 *
 * Set a filter on the #EmpathyAccountChooser included in the
 * #EmpathyContactWidget.
 */
void
empathy_contact_widget_set_account_filter (
    GtkWidget *widget,
    EmpathyAccountChooserFilterFunc filter,
    gpointer user_data)
{
  EmpathyContactWidget *self = EMPATHY_CONTACT_WIDGET (widget);
  EmpathyAccountChooser *chooser;

  chooser = EMPATHY_ACCOUNT_CHOOSER (self->priv->widget_account);
  if (chooser)
      empathy_account_chooser_set_filter (chooser, filter, user_data);
}

static void
empathy_contact_widget_class_init (
    EmpathyContactWidgetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = empathy_contact_widget_finalize;

  g_type_class_add_private (klass, sizeof (EmpathyContactWidgetPriv));
}

static void
empathy_contact_widget_init (EmpathyContactWidget *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_CONTACT_WIDGET, EmpathyContactWidgetPriv);
}
