/*
 * Copyright (C) 2010 Collabora Ltd.
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
 *
 * Authors: Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 */

#include <config.h>

#include "empathy-client-factory.h"

#include "empathy-tp-chat.h"
#include "empathy-utils.h"

G_DEFINE_TYPE (EmpathyClientFactory, empathy_client_factory,
    TP_TYPE_AUTOMATIC_CLIENT_FACTORY)

#define chainup ((TpSimpleClientFactoryClass *) \
    empathy_client_factory_parent_class)

static TpChannel *
empathy_client_factory_create_channel (TpSimpleClientFactory *factory,
    TpConnection *conn,
    const gchar *path,
    const GHashTable *properties,
    GError **error)
{
  const gchar *chan_type;

  chan_type = tp_asv_get_string (properties, TP_PROP_CHANNEL_CHANNEL_TYPE);

  if (!tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
    {
      TpAccount *account;

      account = tp_connection_get_account (conn);

      return TP_CHANNEL (empathy_tp_chat_new (
            TP_SIMPLE_CLIENT_FACTORY (factory), account, conn, path,
            properties));
    }

  return chainup->create_channel (factory, conn, path, properties, error);
}

static GArray *
empathy_client_factory_dup_channel_features (TpSimpleClientFactory *factory,
    TpChannel *channel)
{
  GArray *features;
  GQuark feature;

  features = chainup->dup_channel_features (factory, channel);

  feature = TP_CHANNEL_FEATURE_CONTACTS;
  g_array_append_val (features, feature);

  if (EMPATHY_IS_TP_CHAT (channel))
    {
      feature = TP_TEXT_CHANNEL_FEATURE_CHAT_STATES;
      g_array_append_val (features, feature);

      feature = EMPATHY_TP_CHAT_FEATURE_READY;
      g_array_append_val (features, feature);
    }

  return features;
}

static GArray *
empathy_client_factory_dup_account_features (TpSimpleClientFactory *factory,
    TpAccount *account)
{
  GArray *features;
  GQuark feature;

  features = chainup->dup_account_features (factory, account);

  feature = TP_ACCOUNT_FEATURE_CONNECTION;
  g_array_append_val (features, feature);

  feature = TP_ACCOUNT_FEATURE_ADDRESSING;
  g_array_append_val (features, feature);

  return features;
}

static GArray *
empathy_client_factory_dup_connection_features (TpSimpleClientFactory *factory,
    TpConnection *connection)
{
  GArray *features;
  GQuark feature;

  features = chainup->dup_connection_features (factory, connection);

  feature = TP_CONNECTION_FEATURE_CAPABILITIES;
  g_array_append_val (features, feature);

  feature = TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS;
  g_array_append_val (features, feature);

  feature = TP_CONNECTION_FEATURE_CONTACT_INFO;
  g_array_append_val (features, feature);

  feature = TP_CONNECTION_FEATURE_BALANCE;
  g_array_append_val (features, feature);

  feature = TP_CONNECTION_FEATURE_CONTACT_BLOCKING;
  g_array_append_val (features, feature);

  /* Most empathy-* may allow user to add a contact to his contact list. We
   * need this property to check if the connection allows it. It's cheap to
   * prepare anyway as it will just call GetAll() on the ContactList iface. */
  feature = TP_CONNECTION_FEATURE_CONTACT_LIST_PROPERTIES;
  g_array_append_val (features, feature);

  return features;
}

static GArray *
empathy_client_factory_dup_contact_features (TpSimpleClientFactory *factory,
        TpConnection *connection)
{
  GArray *features;
  TpContactFeature extra_features[] = {
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_PRESENCE,
      TP_CONTACT_FEATURE_AVATAR_TOKEN,
      TP_CONTACT_FEATURE_AVATAR_DATA,
      TP_CONTACT_FEATURE_CAPABILITIES,
      /* Needed by empathy_individual_add_menu_item_new to check if a contact
       * is already in the contact list. This feature is pretty cheap to
       * prepare as it doesn't prepare the full roster. */
      TP_CONTACT_FEATURE_SUBSCRIPTION_STATES,
      TP_CONTACT_FEATURE_CONTACT_GROUPS,
      TP_CONTACT_FEATURE_CLIENT_TYPES,
  };

  features = chainup->dup_contact_features (factory, connection);

  g_array_append_vals (features, extra_features, G_N_ELEMENTS (extra_features));

  return features;
}

static void
empathy_client_factory_class_init (EmpathyClientFactoryClass *cls)
{
  TpSimpleClientFactoryClass *simple_class = (TpSimpleClientFactoryClass *) cls;

  simple_class->create_channel = empathy_client_factory_create_channel;
  simple_class->dup_channel_features =
    empathy_client_factory_dup_channel_features;

  simple_class->dup_account_features =
    empathy_client_factory_dup_account_features;

  simple_class->dup_connection_features =
    empathy_client_factory_dup_connection_features;

  simple_class->dup_contact_features =
    empathy_client_factory_dup_contact_features;
}

static void
empathy_client_factory_init (EmpathyClientFactory *self)
{
}

static EmpathyClientFactory *
empathy_client_factory_new (TpDBusDaemon *dbus)
{
    return g_object_new (EMPATHY_TYPE_CLIENT_FACTORY,
        "dbus-daemon", dbus,
        NULL);
}

EmpathyClientFactory *
empathy_client_factory_dup (void)
{
  static EmpathyClientFactory *singleton = NULL;
  TpDBusDaemon *dbus;
  GError *error = NULL;

  if (singleton != NULL)
    return g_object_ref (singleton);

  dbus = tp_dbus_daemon_dup (&error);
  if (dbus == NULL)
    {
      g_warning ("Failed to get TpDBusDaemon: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  singleton = empathy_client_factory_new (dbus);
  g_object_unref (dbus);

  g_object_add_weak_pointer (G_OBJECT (singleton), (gpointer) &singleton);

  return singleton;
}

static void
dup_contact_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GSimpleAsyncResult *my_result = user_data;
  GError *error = NULL;
  TpContact *contact;

  contact = tp_connection_dup_contact_by_id_finish (TP_CONNECTION (source),
      result, &error);

  if (contact == NULL)
    {
      g_simple_async_result_take_error (my_result, error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (my_result,
          empathy_contact_dup_from_tp_contact (contact), g_object_unref);

      g_object_unref (contact);
    }

  g_simple_async_result_complete (my_result);
  g_object_unref (my_result);
}

void
empathy_client_factory_dup_contact_by_id_async (
    EmpathyClientFactory *self,
    TpConnection *connection,
    const gchar *id,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GArray *features;

  g_return_if_fail (EMPATHY_IS_CLIENT_FACTORY (self));
  g_return_if_fail (id != NULL);

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      empathy_client_factory_dup_contact_by_id_async);

  features = empathy_client_factory_dup_contact_features (
      TP_SIMPLE_CLIENT_FACTORY (self), connection);

  tp_connection_dup_contact_by_id_async (connection, id, features->len,
      (TpContactFeature * ) features->data, dup_contact_cb, result);

  g_array_unref (features);
}

EmpathyContact *
empathy_client_factory_dup_contact_by_id_finish (
    EmpathyClientFactory *self,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_return_copy_pointer (self,
      empathy_client_factory_dup_contact_by_id_async, g_object_ref);
}
