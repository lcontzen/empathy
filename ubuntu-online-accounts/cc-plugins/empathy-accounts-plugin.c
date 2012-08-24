/*
 * empathy-accounts-plugin.c
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

#include "empathy-accounts-plugin.h"

#include <libempathy/empathy-client-factory.h>
#include <libempathy/empathy-uoa-utils.h>

#include "empathy-accounts-plugin-widget.h"

G_DEFINE_TYPE (EmpathyAccountsPlugin, empathy_accounts_plugin, AP_TYPE_PLUGIN)

static void
widget_done_cb (EmpathyAccountsPluginWidget *widget,
    ApPlugin *plugin)
{
  ap_plugin_emit_finished (plugin);
}

static GtkWidget *
empathy_accounts_plugin_build_widget (ApPlugin *plugin)
{
  AgAccount *account;
  GtkWidget *widget;

  account = ap_plugin_get_account (plugin);
  empathy_uoa_manager_set_default (ag_account_get_manager (account));

  widget = empathy_accounts_plugin_widget_new (account);

  g_signal_connect (widget, "done",
      G_CALLBACK (widget_done_cb), plugin);

  return widget;
}

static void
store_delete_cb (AgAccount *account,
    const GError *error,
    gpointer user_data)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      g_debug ("Failed to delete account with ID '%u': %s",
          account->id, error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

static void
empathy_accounts_plugin_delete_account (ApPlugin *plugin,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  AgAccount *account;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (plugin),
      callback, user_data, ap_plugin_delete_account);

  account = ap_plugin_get_account (plugin);

  ag_account_delete (account);
  ag_account_store (account, store_delete_cb, result);
}

static void
empathy_accounts_plugin_act_headless (ApPlugin *plugin)
{
}

static void
empathy_accounts_plugin_class_init (
    EmpathyAccountsPluginClass *klass)
{
  ApPluginClass *pclass = AP_PLUGIN_CLASS (klass);

  pclass->build_widget = empathy_accounts_plugin_build_widget;
  pclass->delete_account = empathy_accounts_plugin_delete_account;
  pclass->act_headless = empathy_accounts_plugin_act_headless;
}

static void
empathy_accounts_plugin_init (EmpathyAccountsPlugin *self)
{
  if (tp_account_manager_can_set_default ())
    {
      EmpathyClientFactory *factory;
      TpAccountManager *am;

      factory = empathy_client_factory_dup ();
      am = tp_account_manager_new_with_factory (
          TP_SIMPLE_CLIENT_FACTORY (factory));
      tp_account_manager_set_default (am);

      g_object_unref (factory);
      g_object_unref (am);
    }

}

GType
ap_module_get_object_type (void)
{
  return EMPATHY_TYPE_ACCOUNTS_PLUGIN;
}
