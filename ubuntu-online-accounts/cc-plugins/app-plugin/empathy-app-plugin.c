/*
 * empathy-app-plugin.c
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

#include "empathy-app-plugin.h"

#include <libempathy/empathy-client-factory.h>

#include "empathy-app-plugin-widget.h"

G_DEFINE_TYPE (EmpathyAppPlugin, empathy_app_plugin, AP_TYPE_APPLICATION_PLUGIN)

static void
widget_done_cb (EmpathyAppPluginWidget *widget,
    ApApplicationPlugin *plugin)
{
  ap_application_plugin_emit_finished (plugin);
}

static GtkWidget *
empathy_app_plugin_build_widget (ApApplicationPlugin *plugin)
{
  GtkWidget *widget;

  widget = empathy_app_plugin_widget_new (
      ap_application_plugin_get_account (plugin));

  g_signal_connect (widget, "done",
      G_CALLBACK (widget_done_cb), plugin);

  gtk_widget_show (widget);

  return widget;
}

static void
empathy_app_plugin_class_init (EmpathyAppPluginClass *klass)
{
  ApApplicationPluginClass *app_class = AP_APPLICATION_PLUGIN_CLASS (klass);

  app_class->build_widget = empathy_app_plugin_build_widget;
}

static void
empathy_app_plugin_init (EmpathyAppPlugin *self)
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
  return EMPATHY_TYPE_APP_PLUGIN;
}
