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

G_DEFINE_TYPE (EmpathyAccountsPluginWidget, empathy_accounts_plugin_widget, GTK_TYPE_BOX)

enum
{
  PROP_ACCOUNT = 1,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyAccountsPluginWidgetPriv
{
  AgAccount *account;
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

static void
empathy_accounts_plugin_widget_constructed (GObject *object)
{
  //EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_accounts_plugin_widget_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_accounts_plugin_widget_dispose (GObject *object)
{
  EmpathyAccountsPluginWidget *self = EMPATHY_ACCOUNTS_PLUGIN_WIDGET (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_accounts_plugin_widget_parent_class)->dispose;

  g_clear_object (&self->priv->account);

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
      NULL);
}
