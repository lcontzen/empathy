/*
 * empathy-auth-uoa.c - Source for Uoa SASL authentication
 * Copyright (C) 2012 Collabora Ltd.
 * @author Xavier Claessens <xavier.claessens@collabora.co.uk>
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

#include <libaccounts-glib/ag-account.h>
#include <libaccounts-glib/ag-account-service.h>
#include <libaccounts-glib/ag-auth-data.h>
#include <libaccounts-glib/ag-manager.h>
#include <libaccounts-glib/ag-service.h>

#include <libsignon-glib/signon-identity.h>
#include <libsignon-glib/signon-auth-session.h>

#define DEBUG_FLAG EMPATHY_DEBUG_SASL
#include "empathy-debug.h"
#include "empathy-utils.h"
#include "empathy-uoa-auth-handler.h"
#include "empathy-sasl-mechanisms.h"

#define SERVICE_TYPE "IM"

struct _EmpathyUoaAuthHandlerPriv
{
  AgManager *manager;
};

G_DEFINE_TYPE (EmpathyUoaAuthHandler, empathy_uoa_auth_handler, G_TYPE_OBJECT);

static void
empathy_uoa_auth_handler_init (EmpathyUoaAuthHandler *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_UOA_AUTH_HANDLER, EmpathyUoaAuthHandlerPriv);

  self->priv->manager = ag_manager_new_for_service_type (SERVICE_TYPE);
}

static void
empathy_uoa_auth_handler_dispose (GObject *object)
{
  EmpathyUoaAuthHandler *self = (EmpathyUoaAuthHandler *) object;

  tp_clear_object (&self->priv->manager);

  G_OBJECT_CLASS (empathy_uoa_auth_handler_parent_class)->dispose (object);
}

static void
empathy_uoa_auth_handler_class_init (EmpathyUoaAuthHandlerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = empathy_uoa_auth_handler_dispose;

  g_type_class_add_private (klass, sizeof (EmpathyUoaAuthHandlerPriv));
}

EmpathyUoaAuthHandler *
empathy_uoa_auth_handler_new (void)
{
  return g_object_new (EMPATHY_TYPE_UOA_AUTH_HANDLER, NULL);
}

static void
auth_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) source;
  GError *error = NULL;

  if (!empathy_sasl_auth_finish (channel, result, &error))
    {
      DEBUG ("SASL Mechanism error: %s", error->message);
      g_clear_error (&error);
    }

  tp_channel_close_async (channel, NULL, NULL);
}

typedef struct
{
  TpChannel *channel;
  AgAuthData *auth_data;
  SignonAuthSession *session;
  SignonIdentity *identity;

  gchar *username;
} QueryInfoData;

static QueryInfoData *
query_info_data_new (TpChannel *channel,
    AgAuthData *auth_data,
    SignonAuthSession *session,
    SignonIdentity *identity)
{
  QueryInfoData *data;

  data = g_slice_new0 (QueryInfoData);
  data->channel = g_object_ref (channel);
  data->auth_data = ag_auth_data_ref (auth_data);
  data->session = g_object_ref (session);
  data->identity = g_object_ref (identity);

  return data;
}

static void
query_info_data_free (QueryInfoData *data)
{
  g_object_unref (data->channel);
  ag_auth_data_unref (data->auth_data);
  g_object_unref (data->session);
  g_object_unref (data->identity);
  g_free (data->username);
  g_slice_free (QueryInfoData, data);
}

static void
session_process_cb (SignonAuthSession *session,
    GHashTable *session_data,
    const GError *error,
    gpointer user_data)
{
  QueryInfoData *data = user_data;
  const gchar *access_token;
  const gchar *client_id;

  if (error != NULL)
    {
      DEBUG ("Error processing the session: %s", error->message);
      tp_channel_close_async (data->channel, NULL, NULL);
      query_info_data_free (data);
      return;
    }

  access_token = tp_asv_get_string (session_data, "AccessToken");
  client_id = tp_asv_get_string (ag_auth_data_get_parameters (data->auth_data),
      "ClientId");

  switch (empathy_sasl_channel_select_mechanism (data->channel))
    {
      case EMPATHY_SASL_MECHANISM_FACEBOOK:
        empathy_sasl_auth_facebook_async (data->channel,
            client_id, access_token,
            auth_cb, NULL);
        break;

      case EMPATHY_SASL_MECHANISM_WLM:
        empathy_sasl_auth_wlm_async (data->channel,
            access_token,
            auth_cb, NULL);
        break;

      case EMPATHY_SASL_MECHANISM_GOOGLE:
        empathy_sasl_auth_google_async (data->channel,
            data->username, access_token,
            auth_cb, NULL);
        break;

      default:
        g_assert_not_reached ();
    }

  query_info_data_free (data);
}

static void
identity_query_info_cb (SignonIdentity *identity,
    const SignonIdentityInfo *info,
    const GError *error,
    gpointer user_data)
{
  QueryInfoData *data = user_data;

  if (error != NULL)
    {
      DEBUG ("Error querying info from identity: %s", error->message);
      tp_channel_close_async (data->channel, NULL, NULL);
      query_info_data_free (data);
      return;
    }

  data->username = g_strdup (signon_identity_info_get_username (info));

  signon_auth_session_process (data->session,
      ag_auth_data_get_parameters (data->auth_data),
      ag_auth_data_get_mechanism (data->auth_data),
      session_process_cb,
      data);
}

void
empathy_uoa_auth_handler_start (EmpathyUoaAuthHandler *self,
    TpChannel *channel,
    TpAccount *tp_account)
{
  const GValue *id_value;
  AgAccountId id;
  AgAccount *account;
  GList *l = NULL;
  AgAccountService *service;
  AgAuthData *auth_data;
  guint cred_id;
  SignonIdentity *identity;
  SignonAuthSession *session;
  GError *error = NULL;

  g_return_if_fail (TP_IS_CHANNEL (channel));
  g_return_if_fail (TP_IS_ACCOUNT (tp_account));
  g_return_if_fail (empathy_uoa_auth_handler_supports (self, channel,
      tp_account));

  DEBUG ("Start UOA auth for account: %s",
      tp_proxy_get_object_path (tp_account));

  id_value = tp_account_get_storage_identifier (tp_account);
  id = g_value_get_uint (id_value);

  account = ag_manager_get_account (self->priv->manager, id);
  if (account != NULL)
    l = ag_account_list_services_by_type (account, SERVICE_TYPE);
  if (l == NULL)
    {
      DEBUG ("Couldn't find IM service for AgAccountId %u", id);
      g_object_unref (account);
      tp_channel_close_async (channel, NULL, NULL);
      return;
    }

  /* Assume there is only one IM service */
  service = ag_account_service_new (account, l->data);
  ag_service_list_free (l);
  g_object_unref (account);

  auth_data = ag_account_service_get_auth_data (service);
  cred_id = ag_auth_data_get_credentials_id (auth_data);
  identity = signon_identity_new_from_db (cred_id);
  session = signon_identity_create_session (identity,
      ag_auth_data_get_method (auth_data),
      &error);
  if (session == NULL)
    {
      DEBUG ("Error creating a SignonAuthSession: %s", error->message);
      tp_channel_close_async (channel, NULL, NULL);
      goto cleanup;
    }

  /* Query UOA for more info */
  signon_identity_query_info (identity,
      identity_query_info_cb,
      query_info_data_new (channel, auth_data, session, identity));

cleanup:
  ag_auth_data_unref (auth_data);
  g_object_unref (service);
  g_object_unref (identity);
  g_object_unref (session);
}

gboolean
empathy_uoa_auth_handler_supports (EmpathyUoaAuthHandler *self,
    TpChannel *channel,
    TpAccount *account)
{
  const gchar *provider;
  EmpathySaslMechanism mech;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);

  provider = tp_account_get_storage_provider (account);

  if (tp_strdiff (provider, EMPATHY_UOA_PROVIDER))
    return FALSE;

  mech = empathy_sasl_channel_select_mechanism (channel);
  return mech == EMPATHY_SASL_MECHANISM_FACEBOOK ||
      mech == EMPATHY_SASL_MECHANISM_WLM ||
      mech == EMPATHY_SASL_MECHANISM_GOOGLE;
}
