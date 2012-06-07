/*
 * empathy-new-account-dialog.c - Source for EmpathyNewAccountDialog
 *
 * Copyright (C) 2011 - Collabora Ltd.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with This library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "empathy-new-account-dialog.h"

#include <glib/gi18n-lib.h>

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-account-widget.h>
#include <libempathy-gtk/empathy-protocol-chooser.h>

G_DEFINE_TYPE (EmpathyNewAccountDialog, empathy_new_account_dialog, \
    GTK_TYPE_DIALOG)

struct _EmpathyNewAccountDialogPrivate
{
  GtkWidget *chooser;
  EmpathyAccountWidget *current_account_widget;
  GtkWidget *main_vbox;
  GtkWidget *connect_button;

  EmpathyAccountSettings *settings;
};

static void
close_cb (EmpathyAccountWidget *widget,
    GtkResponseType response,
    EmpathyNewAccountDialog *self)
{
  gtk_dialog_response (GTK_DIALOG (self), response);
}

static void
protocol_changed_cb (GtkComboBox *chooser,
    EmpathyNewAccountDialog *self)
{
  EmpathyAccountSettings *settings;
  EmpathyAccountWidget *account_widget;
  gchar *password = NULL, *account = NULL;

  settings = empathy_protocol_chooser_create_account_settings (
      EMPATHY_PROTOCOL_CHOOSER (chooser));

  if (settings == NULL)
    return;

  /* Save "account" and "password" parameters */
  if (self->priv->settings != NULL)
    {
      account = empathy_account_settings_dup_string (
            self->priv->settings, "account");

      password = empathy_account_settings_dup_string (
            self->priv->settings, "password");

      g_object_unref (self->priv->settings);
    }

  account_widget = empathy_account_widget_new_for_protocol (settings, TRUE);

  if (self->priv->current_account_widget != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->priv->current_account_widget,
          close_cb, self);

      gtk_widget_destroy (GTK_WIDGET (self->priv->current_account_widget));
    }

  self->priv->current_account_widget = account_widget;

  self->priv->settings = settings;

  g_signal_connect (self->priv->current_account_widget, "close",
      G_CALLBACK (close_cb), self);

  /* Restore "account" and "password" parameters in the new widget */
  if (account != NULL)
    {
      empathy_account_widget_set_account_param (account_widget, account);
      g_free (account);
    }

  if (password != NULL)
    {
      empathy_account_widget_set_password_param (account_widget, password);
      g_free (password);
    }

  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox),
      GTK_WIDGET (account_widget), FALSE, FALSE, 0);
  gtk_widget_show (GTK_WIDGET (account_widget));
}

static void
empathy_new_account_dialog_init (EmpathyNewAccountDialog *self)
{
  GtkWidget *w, *hbox, *content;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_NEW_ACCOUNT_DIALOG, EmpathyNewAccountDialogPrivate);

  self->priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (self->priv->main_vbox), 12);
  gtk_widget_show (self->priv->main_vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  w = gtk_label_new (_("What kind of chat account do you have?"));
  gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
  gtk_widget_show (w);

  w = gtk_alignment_new (0, 0, 0, 0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (w), 0, 0, 12, 0);
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), w, FALSE, FALSE, 0);
  gtk_widget_show (w);

  self->priv->chooser = empathy_protocol_chooser_new ();
  gtk_box_pack_start (GTK_BOX (hbox), self->priv->chooser, FALSE, FALSE, 0);
  gtk_widget_show (self->priv->chooser);

  content = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_container_add (GTK_CONTAINER (content), self->priv->main_vbox);

  g_signal_connect (self->priv->chooser, "changed",
      G_CALLBACK (protocol_changed_cb), self);

  /* trigger show the first account widget */
  protocol_changed_cb (GTK_COMBO_BOX (self->priv->chooser), self);

  gtk_window_set_title (GTK_WINDOW (self), _("Adding new account"));
}

static void
empathy_new_account_dialog_dispose (GObject *object)
{
  EmpathyNewAccountDialog *self = (EmpathyNewAccountDialog *) object;

  g_clear_object (&self->priv->settings);

  G_OBJECT_CLASS (empathy_new_account_dialog_parent_class)->dispose (object);
}

static void
empathy_new_account_dialog_class_init (EmpathyNewAccountDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = empathy_new_account_dialog_dispose;

  g_type_class_add_private (object_class,
      sizeof (EmpathyNewAccountDialogPrivate));
}

GtkWidget *
empathy_new_account_dialog_new (GtkWindow *parent)
{
  GtkWidget *result;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  result = g_object_new (EMPATHY_TYPE_NEW_ACCOUNT_DIALOG,
      "modal", TRUE,
      "destroy-with-parent", TRUE,
      NULL);

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  return result;
}

EmpathyAccountSettings *
empathy_new_account_dialog_get_settings (EmpathyNewAccountDialog *self)
{
  return self->priv->settings;
}
