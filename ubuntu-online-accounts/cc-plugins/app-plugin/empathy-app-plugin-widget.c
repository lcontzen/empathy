/*
 * empathy-app-plugin-widget.c
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

#include <glib/gi18n-lib.h>

#include <telepathy-glib/telepathy-glib.h>

#include <libaccounts-glib/ag-manager.h>
#include <libaccounts-glib/ag-provider.h>

#include <libempathy/empathy-contact.h>
#include <libempathy-gtk/empathy-user-info.h>

#include "empathy-app-plugin-widget.h"

G_DEFINE_TYPE (EmpathyAppPluginWidget, empathy_app_plugin_widget, GTK_TYPE_BOX)

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

struct _EmpathyAppPluginWidgetPriv
{
  AgAccount *account;

  GtkWidget *user_info;
};

static void
empathy_app_plugin_widget_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyAppPluginWidget *self = EMPATHY_APP_PLUGIN_WIDGET (object);

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
empathy_app_plugin_widget_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyAppPluginWidget *self = EMPATHY_APP_PLUGIN_WIDGET (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_assert (self->priv->account == NULL); /* construct only */
        self->priv->account = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
response_cb (GtkWidget *widget,
    gint response,
    EmpathyAppPluginWidget *self)
{
  if (self->priv->user_info != NULL)
    {
      EmpathyUserInfo *user_info = (EmpathyUserInfo *) self->priv->user_info;

      if (response == GTK_RESPONSE_OK)
        empathy_user_info_apply_async (user_info, NULL, NULL);
      else
        empathy_user_info_discard (user_info);
    }

  g_signal_emit (self, signals[SIG_DONE], 0);
}

static GtkWidget *
create_top_bar (EmpathyAppPluginWidget *self)
{
  GtkWidget *bar, *content, *action, *label;
  GtkCssProvider *css;
  AgProvider *provider;
  gchar *str;
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

  provider = ag_manager_get_provider (
      ag_account_get_manager (self->priv->account),
      ag_account_get_provider_name (self->priv->account));
  str = g_strdup_printf (_("Edit %s account options"),
      ag_provider_get_display_name (provider));
  label = gtk_label_new (str);
  gtk_container_add (GTK_CONTAINER (content), label);
  gtk_widget_show (label);
  ag_provider_unref (provider);
  g_free (str);

  g_signal_connect (bar, "response",
      G_CALLBACK (response_cb), self);

  return bar;
}

static void
manager_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  EmpathyAppPluginWidget *self = user_data;
  TpAccountManager *manager = (TpAccountManager *) source;
  GList *accounts;
  GError *error = NULL;

  if (!tp_account_manager_prepare_all_finish (manager, result, &error))
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
          GtkWidget *alig;

          alig = gtk_alignment_new (0.5, 0, 0, 0);
          self->priv->user_info = empathy_user_info_new (account);
          gtk_container_add (GTK_CONTAINER (alig), self->priv->user_info);
          gtk_widget_show (self->priv->user_info);

          gtk_box_pack_start (GTK_BOX (self), alig, TRUE, TRUE, 0);
          gtk_widget_show (alig);
          break;
        }

      accounts = g_list_delete_link (accounts, accounts);
    }
  g_list_free (accounts);

out:
  g_object_unref (self);
}

static void
empathy_app_plugin_widget_constructed (GObject *object)
{
  EmpathyAppPluginWidget *self = EMPATHY_APP_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_app_plugin_widget_parent_class)->constructed;
  GtkWidget *top;
  TpAccountManager *manager;
  TpSimpleClientFactory *factory;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (AG_IS_ACCOUNT (self->priv->account));

  /* Top bar */
  top = create_top_bar (self);
  gtk_widget_show (top);
  gtk_box_pack_start (GTK_BOX (self), top, FALSE, FALSE, 0);

  /* Prepare tp's account manager to find the TpAccount corresponding to our
   * AgAccount */
  manager = tp_account_manager_dup ();
  factory = tp_proxy_get_factory (manager);
  tp_simple_client_factory_add_account_features_varargs (factory,
      TP_ACCOUNT_FEATURE_STORAGE,
      TP_ACCOUNT_FEATURE_CONNECTION,
      0);
  tp_simple_client_factory_add_connection_features_varargs (factory,
      TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS,
      TP_CONNECTION_FEATURE_CONTACT_INFO,
      0);
  tp_simple_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_DATA,
      TP_CONTACT_FEATURE_CONTACT_INFO,
      TP_CONTACT_FEATURE_INVALID,
      0);
  tp_account_manager_prepare_all_async (manager,
      manager_prepared_cb, g_object_ref (self));
  g_object_unref (manager);
}

static void
empathy_app_plugin_widget_dispose (GObject *object)
{
  EmpathyAppPluginWidget *self = EMPATHY_APP_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_app_plugin_widget_parent_class)->dispose;

  g_clear_object (&self->priv->account);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_app_plugin_widget_class_init (
    EmpathyAppPluginWidgetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_app_plugin_widget_get_property;
  oclass->set_property = empathy_app_plugin_widget_set_property;
  oclass->constructed = empathy_app_plugin_widget_constructed;
  oclass->dispose = empathy_app_plugin_widget_dispose;

  spec = g_param_spec_object ("account", "account",
      "Account",
      AG_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_ACCOUNT, spec);

  signals[SIG_DONE] = g_signal_new ("done",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      0);

  g_type_class_add_private (klass, sizeof (EmpathyAppPluginWidgetPriv));
}

static void
empathy_app_plugin_widget_init (EmpathyAppPluginWidget *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_APP_PLUGIN_WIDGET, EmpathyAppPluginWidgetPriv);
}

GtkWidget *
empathy_app_plugin_widget_new (AgAccount *account)
{
  return g_object_new (EMPATHY_TYPE_APP_PLUGIN_WIDGET,
      "account", account,
      "orientation", GTK_ORIENTATION_VERTICAL,
      "spacing", 10,
      NULL);
}
