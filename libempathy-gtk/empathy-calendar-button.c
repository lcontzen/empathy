/*
 * empathy-calendar-button.c - Source for EmpathyCalendarButton
 * Copyright (C) 2012 Collabora Ltd.
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "empathy-calendar-button.h"

#include <libempathy/empathy-utils.h>

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER_THING
#include <libempathy/empathy-debug.h>

G_DEFINE_TYPE (EmpathyCalendarButton, empathy_calendar_button, GTK_TYPE_BOX)

/* signal enum */
enum {
  DATE_CHANGED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

struct _EmpathyCalendarButtonPriv {
  GDate *date;

  GtkWidget *button_date;
  GtkWidget *button_clear;
  GtkWidget *dialog;
  GtkWidget *calendar;
};

static void
empathy_calendar_button_finalize (GObject *object)
{
  EmpathyCalendarButton *self = (EmpathyCalendarButton *) object;

  tp_clear_pointer (&self->priv->date, g_date_free);

  G_OBJECT_CLASS (empathy_calendar_button_parent_class)->finalize (object);
}

static void
update_label (EmpathyCalendarButton *self)
{
  if (self->priv->date == NULL)
    {
      gtk_button_set_label (GTK_BUTTON (self->priv->button_date),
          _("Select..."));
    }
  else
    {
      gchar buffer[128];

      g_date_strftime (buffer, 128, "%e %b %Y", self->priv->date);
      gtk_button_set_label (GTK_BUTTON (self->priv->button_date), buffer);
    }
}

static void
empathy_calendar_button_constructed (GObject *object)
{
  EmpathyCalendarButton *self = (EmpathyCalendarButton *) object;

  G_OBJECT_CLASS (empathy_calendar_button_parent_class)->constructed (
      object);

  update_label (self);
}

static void
dialog_response (GtkDialog *dialog,
    gint response,
    EmpathyCalendarButton *self)
{
  GDate *date;
  guint year, month, day;

  if (response != GTK_RESPONSE_OK)
    goto out;

  gtk_calendar_get_date (GTK_CALENDAR (self->priv->calendar),
      &year, &month, &day);
  date = g_date_new_dmy (day, month + 1, year);

  empathy_calendar_button_set_date (self, date);

  g_date_free (date);

out:
  gtk_widget_hide (GTK_WIDGET (dialog));
}

static gboolean
dialog_destroy (GtkWidget *widget,
    EmpathyCalendarButton *self)
{
  self->priv->dialog = NULL;
  self->priv->calendar = NULL;

  return FALSE;
}

static void
update_calendar (EmpathyCalendarButton *self)
{
  if (self->priv->calendar == NULL)
    return;

  gtk_calendar_clear_marks (GTK_CALENDAR (self->priv->calendar));

  if (self->priv->date == NULL)
    return;

  gtk_calendar_select_day (GTK_CALENDAR (self->priv->calendar),
      g_date_get_day (self->priv->date));
  gtk_calendar_select_month (GTK_CALENDAR (self->priv->calendar),
      g_date_get_month (self->priv->date) - 1,
      g_date_get_year (self->priv->date));
  gtk_calendar_mark_day (GTK_CALENDAR (self->priv->calendar),
      g_date_get_day (self->priv->date));
}

static void
empathy_calendar_button_date_clicked (GtkButton *button,
    EmpathyCalendarButton *self)
{
  if (self->priv->dialog == NULL)
    {
      GtkWidget *parent, *content;

      parent = gtk_widget_get_toplevel (GTK_WIDGET (button));

      self->priv->dialog = gtk_dialog_new_with_buttons (NULL,
          GTK_WINDOW (parent), GTK_DIALOG_MODAL,
          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
          _("_Select"), GTK_RESPONSE_OK,
          NULL);

      gtk_window_set_transient_for (GTK_WINDOW (self->priv->dialog),
          GTK_WINDOW (parent));

      self->priv->calendar = gtk_calendar_new ();

      update_calendar (self);

      content = gtk_dialog_get_content_area (GTK_DIALOG (self->priv->dialog));

      gtk_box_pack_start (GTK_BOX (content), self->priv->calendar, TRUE, TRUE,
          6);
      gtk_widget_show (self->priv->calendar);

      g_signal_connect (self->priv->dialog, "response",
          G_CALLBACK (dialog_response), self);
      g_signal_connect (self->priv->dialog, "destroy",
          G_CALLBACK (dialog_destroy), self);
    }

  gtk_window_present (GTK_WINDOW (self->priv->dialog));
}

static void
empathy_calendar_button_clear_clicked (GtkButton *button,
    EmpathyCalendarButton *self)
{
  empathy_calendar_button_set_date (self, NULL);
}

static void
empathy_calendar_button_init (EmpathyCalendarButton *self)
{
  GtkWidget *image;
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_CALENDAR_BUTTON, EmpathyCalendarButtonPriv);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

  /* Date */
  self->priv->button_date = gtk_button_new ();

  g_signal_connect (self->priv->button_date, "clicked",
      G_CALLBACK (empathy_calendar_button_date_clicked), self);

  gtk_button_set_alignment (GTK_BUTTON (self->priv->button_date), 0, 0.5);

  gtk_box_pack_start (GTK_BOX (self), self->priv->button_date, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->button_date);

  /* Clear */
  self->priv->button_clear = gtk_button_new ();

  image = gtk_image_new_from_stock (GTK_STOCK_CLEAR,
      GTK_ICON_SIZE_MENU);
  gtk_button_set_image (GTK_BUTTON (self->priv->button_clear), image);
  gtk_widget_show (image);

  g_signal_connect (self->priv->button_clear, "clicked",
      G_CALLBACK (empathy_calendar_button_clear_clicked), self);

  gtk_box_pack_start (GTK_BOX (self), self->priv->button_clear,
      FALSE, FALSE, 0);
  gtk_widget_show (self->priv->button_clear);
}

static void
empathy_calendar_button_class_init (EmpathyCalendarButtonClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EmpathyCalendarButtonPriv));

  oclass->finalize = empathy_calendar_button_finalize;
  oclass->constructed = empathy_calendar_button_constructed;

  signals[DATE_CHANGED] = g_signal_new ("date-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0,
      NULL, NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE, 1, G_TYPE_DATE);
}

GtkWidget *
empathy_calendar_button_new (void)
{
  return g_object_new (EMPATHY_TYPE_CALENDAR_BUTTON,
      "orientation", GTK_ORIENTATION_HORIZONTAL,
      NULL);
}

GDate *
empathy_calendar_button_get_date (EmpathyCalendarButton *self)
{
  return self->priv->date;
}

void
empathy_calendar_button_set_date (EmpathyCalendarButton *self,
    GDate *date)
{
  if (date == self->priv->date)
    return;

  tp_clear_pointer (&self->priv->date, g_date_free);

  if (date != NULL)
    {
      /* There is no g_date_copy()... */
      self->priv->date = g_date_new_dmy (date->day, date->month, date->year);
    }

  update_label (self);
  update_calendar (self);

  g_signal_emit (self, signals[DATE_CHANGED], 0, self->priv->date);
}
