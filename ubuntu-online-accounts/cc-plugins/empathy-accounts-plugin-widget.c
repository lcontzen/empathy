/*
 * empathy-accounts-plugin-widget.c
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "empathy-accounts-plugin-widget.h"

#include <glib/gi18n-lib.h>

#include <telepathy-glib/telepathy-glib.h>

#include <libaccounts-glib/ag-service.h>
#include <libaccounts-glib/ag-account-service.h>

#include <libempathy-gtk/empathy-account-widget.h>

G_DEFINE_TYPE (EmpathyAccountsPluginWidget, empathy_accounts_plugin_widget, GTK_TYPE_BOX)

enum
{
  PROP_ACCOUNT = 1,
  N_PROPS
};

enum
{
  SIG_DONE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _EmpathyAccountsPluginWidgetPriv
{
  AgAccount *account;

  EmpathyAccountSettings *settings;

  EmpathyAccountWidget *account_widget;
  GtkWidget *done_button;
};

static void
empathy_accounts_plugin_widget_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, self->priv->account);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_accounts_plugin_widget_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        self->priv->account = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static EmpathyAccountSettings *
create_account_settings (AgAccount *account)
{
  AgService *service;
  GList *services;
  AgAccountService *account_service;
  GValue v = G_VALUE_INIT;
  gchar *manager = NULL, *protocol = NULL;
  EmpathyAccountSettings *settings;

  g_assert (account->id == 0);

  services = ag_account_list_services_by_type (account, "IM");
  g_return_val_if_fail (services != NULL, NULL);
  service = (AgService *) services->data;

  account_service = ag_account_service_new (account, service);

  g_value_init (&v, G_TYPE_STRING);
  if (ag_account_service_get_value (account_service,
        "telepathy/manager", &v) != AG_SETTING_SOURCE_NONE)
    manager = g_value_dup_string (&v);
  g_value_unset (&v);

  g_value_init (&v, G_TYPE_STRING);
  if (ag_account_service_get_value (account_service,
        "telepathy/protocol", &v) != AG_SETTING_SOURCE_NONE)
    protocol = g_value_dup_string (&v);
  g_value_unset (&v);

  g_return_val_if_fail (manager != NULL, NULL);
  g_return_val_if_fail (protocol != NULL, NULL);

  settings = empathy_account_settings_new (manager, protocol, NULL,
      ag_service_get_display_name (service));

  empathy_account_settings_set_storage_provider (settings,
      EMPATHY_UOA_PROVIDER);

  empathy_account_settings_set_icon_name_async (settings,
    ag_service_get_icon_name (service), NULL, NULL);

  g_free (manager);
  g_free (protocol);

  return settings;
}

static void
response_cb (GtkWidget *widget,
    gint response,
    EmpathyAccountsPluginWidget *self)
{
  if (response == GTK_RESPONSE_OK)
    {
      empathy_account_widget_apply_and_log_in (self->priv->account_widget);

      /* Rely on account_widget_close_cb to fire the 'done' signal */
    }
  else
    {
      empathy_account_widget_discard_pending_changes (
          self->priv->account_widget);

      g_signal_emit (self, signals[SIG_DONE], 0);
    }
}

static GtkWidget *
create_top_bar (EmpathyAccountsPluginWidget *self)
{
  GtkWidget *bar, *content, *action, *label;
  GtkCssProvider *css;
  GError *error = NULL;

  bar = gtk_info_bar_new_with_buttons (
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      _("Done"), GTK_RESPONSE_OK,
      NULL);
  gtk_widget_set_hexpand (bar, TRUE);
  gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_QUESTION);
  action = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (action),
      GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_name (bar, "authorization-infobar");
  css = gtk_css_provider_new ();
  if (gtk_css_provider_load_from_data (css,
          "@define-color question_bg_color rgb (222, 222, 222);"
          "GtkInfoBar#authorization-infobar"
          "{"
          "  color: @fg_color;"
          "}",
          -1, &error))
    {
      GtkStyleContext *context = gtk_widget_get_style_context (bar);

      gtk_style_context_add_provider (context, (GtkStyleProvider *) css,
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  else
    {
      g_warning ("Error processing CSS theme override: %s", error->message);
      g_clear_error (&error);
    }
  g_object_unref (css);

  content = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));

  label = gtk_label_new (_("Please enter your account details"));
  gtk_container_add (GTK_CONTAINER (content), label);
  gtk_widget_show (label);

  g_signal_connect (bar, "response",
      G_CALLBACK (response_cb), self);

  return bar;
}

static void
account_widget_handle_apply_cb (EmpathyAccountWidget *widget,
    gboolean valid,
    EmpathyAccountsPluginWidget *self)
{
  gtk_widget_set_sensitive (self->priv->done_button, valid);
}

static void
account_widget_close_cb (EmpathyAccountWidget *widget,
    GtkResponseType response,
    EmpathyAccountsPluginWidget *self)
{
  g_signal_emit (self, signals[SIG_DONE], 0);
}

static void
add_account_widget (EmpathyAccountsPluginWidget *self)
{
  GtkWidget *alig;

  alig = gtk_alignment_new (0.5, 0, 0, 0);

  gtk_box_pack_start (GTK_BOX (self), alig, TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (alig));

  self->priv->account_widget = empathy_account_widget_new_for_protocol (
      self->priv->settings, TRUE);

  empathy_account_widget_hide_buttons (self->priv->account_widget);

  gtk_widget_set_valign (GTK_WIDGET (self->priv->account_widget),
      GTK_ALIGN_CENTER);

  gtk_container_add (GTK_CONTAINER (alig),
      GTK_WIDGET (self->priv->account_widget));
  gtk_widget_show (GTK_WIDGET (self->priv->account_widget));

  if (!empathy_account_settings_is_valid (self->priv->settings))
    {
      gtk_widget_set_sensitive (self->priv->done_button, FALSE);
    }

  g_signal_connect (self->priv->account_widget, "handle-apply",
      G_CALLBACK (account_widget_handle_apply_cb), self);
  g_signal_connect (self->priv->account_widget, "close",
      G_CALLBACK (account_widget_close_cb), self);
}

static void
maybe_add_account_widget (EmpathyAccountsPluginWidget *self)
{
  g_return_if_fail (self->priv->settings != NULL);

  if (empathy_account_settings_is_ready (self->priv->settings))
    {
      add_account_widget (self);
    }
  else
    {
      g_signal_connect_swapped (self->priv->settings, "notify::ready",
          G_CALLBACK (add_account_widget), self);
    }
}

static void
manager_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  EmpathyAccountsPluginWidget *self = user_data;
  TpAccountManager *manager = (TpAccountManager *) source;
  GList *accounts;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (manager, result, &error))
    {
      g_debug ("Error preparing Account Manager: %s", error->message);
      g_clear_error (&error);
      goto out;
    }

  accounts = tp_account_manager_get_valid_accounts (manager);
  while (accounts != NULL)
    {
      TpAccount *account = accounts->data;
      const GValue *value;

      value = tp_account_get_storage_identifier (account);
      if (G_VALUE_HOLDS_UINT (value) &&
          g_value_get_uint (value) == self->priv->account->id)
        {
          self->priv->settings = empathy_account_settings_new_for_account (
              account);
          maybe_add_account_widget (self);
          break;
        }

      accounts = g_list_delete_link (accounts, accounts);
    }
  g_list_free (accounts);

out:
  g_object_unref (self);
}

static void
empathy_accounts_plugin_widget_constructed (GObject *object)
{
  EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_accounts_plugin_widget_parent_class)->constructed;
  GtkWidget *top;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (AG_IS_ACCOUNT (self->priv->account));

  /* Top bar */
  top = create_top_bar (self);
  gtk_widget_show (top);
  gtk_box_pack_start (GTK_BOX (self), top, FALSE, FALSE, 0);

  if (self->priv->account->id != 0)
    {
      TpAccountManager *manager;

      /* Prepare tp's account manager to find the TpAccount corresponding to our
       * AgAccount */
      manager = tp_account_manager_dup ();

      tp_proxy_prepare_async (manager, NULL,
          manager_prepared_cb, g_object_ref (self));
      g_object_unref (manager);
      return;
    }

  self->priv->settings = create_account_settings (self->priv->account);
  maybe_add_account_widget (self);
}

static void
empathy_accounts_plugin_widget_dispose (GObject *object)
{
  EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_accounts_plugin_widget_parent_class)->dispose;

  g_clear_object (&self->priv->account);
  g_clear_object (&self->priv->settings);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_accounts_plugin_widget_class_init (
    EmpathyAccountsPluginWidgetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_accounts_plugin_widget_get_property;
  oclass->set_property = empathy_accounts_plugin_widget_set_property;
  oclass->constructed = empathy_accounts_plugin_widget_constructed;
  oclass->dispose = empathy_accounts_plugin_widget_dispose;

  spec = g_param_spec_object ("account", "account",
      "AgAccount",
      AG_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_ACCOUNT, spec);

  signals[SIG_DONE] = g_signal_new ("done",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      0);

  g_type_class_add_private (klass, sizeof (EmpathyAccountsPluginWidgetPriv));
}

static void
empathy_accounts_plugin_widget_init (EmpathyAccountsPluginWidget *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ACCOUNTS_PLUGIN_WIDGET, EmpathyAccountsPluginWidgetPriv);
}

GtkWidget *
empathy_accounts_plugin_widget_new (AgAccount *account)
{
  return g_object_new (EMPATHY_TYPE_ACCOUNTS_PLUGIN_WIDGET,
      "account", account,
      "orientation", GTK_ORIENTATION_VERTICAL,
      "spacing", 10,
      NULL);
}
