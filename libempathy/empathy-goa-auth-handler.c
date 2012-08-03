/*
 * empathy-goa-auth-handler.c - Source for Goa SASL authentication
 * Copyright (C) 2011 Collabora Ltd.
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

#define GOA_API_IS_SUBJECT_TO_CHANGE /* awesome! */
#include <goa/goa.h>

#define DEBUG_FLAG EMPATHY_DEBUG_SASL
#include "empathy-debug.h"
#include "empathy-utils.h"
#include "empathy-goa-auth-handler.h"
#include "empathy-sasl-mechanisms.h"

struct _EmpathyGoaAuthHandlerPriv
{
  GoaClient *client;
  gboolean client_preparing;

  /* List of AuthData waiting for client to be created */
  GList *auth_queue;
};

G_DEFINE_TYPE (EmpathyGoaAuthHandler, empathy_goa_auth_handler, G_TYPE_OBJECT);

static void
empathy_goa_auth_handler_init (EmpathyGoaAuthHandler *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_GOA_AUTH_HANDLER, EmpathyGoaAuthHandlerPriv);
}

static void
empathy_goa_auth_handler_dispose (GObject *object)
{
  EmpathyGoaAuthHandler *self = (EmpathyGoaAuthHandler *) object;

  /* AuthData keeps a ref on self */
  g_assert (self->priv->auth_queue == NULL);

  tp_clear_object (&self->priv->client);

  G_OBJECT_CLASS (empathy_goa_auth_handler_parent_class)->dispose (object);
}

static void
empathy_goa_auth_handler_class_init (EmpathyGoaAuthHandlerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = empathy_goa_auth_handler_dispose;

  g_type_class_add_private (klass, sizeof (EmpathyGoaAuthHandlerPriv));
}

EmpathyGoaAuthHandler *
empathy_goa_auth_handler_new (void)
{
  return g_object_new (EMPATHY_TYPE_GOA_AUTH_HANDLER, NULL);
}

typedef struct
{
  EmpathyGoaAuthHandler *self;
  TpChannel *channel;
  TpAccount *account;

  GoaObject *goa_object;
  gchar *access_token;
} AuthData;

static AuthData *
auth_data_new (EmpathyGoaAuthHandler *self,
    TpChannel *channel,
    TpAccount *account)
{
  AuthData *data;

  data = g_slice_new0 (AuthData);
  data->self = g_object_ref (self);
  data->channel = g_object_ref (channel);
  data->account = g_object_ref (account);

  return data;
}

static void
auth_data_free (AuthData *data)
{
  tp_clear_object (&data->self);
  tp_clear_object (&data->channel);
  tp_clear_object (&data->account);
  tp_clear_object (&data->goa_object);
  g_free (data->access_token);
  g_slice_free (AuthData, data);
}

static void
fail_auth (AuthData *data)
{
  DEBUG ("Auth failed for account %s",
      tp_proxy_get_object_path (data->account));

  tp_channel_close_async (data->channel, NULL, NULL);
  auth_data_free (data);
}

static void
auth_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) source;
  AuthData *data = user_data;
  GError *error = NULL;

  if (!empathy_sasl_auth_finish (channel, result, &error))
    {
      DEBUG ("SASL Mechanism error: %s", error->message);
      fail_auth (data);
      g_clear_error (&error);
      return;
    }

  /* Success! */
  tp_channel_close_async (channel, NULL, NULL);
  auth_data_free (data);
}

static void
got_oauth2_access_token_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GoaOAuth2Based *oauth2 = (GoaOAuth2Based *) source;
  AuthData *data = user_data;
  gchar *access_token;
  gint expires_in;
  GError *error = NULL;

  if (!goa_oauth2_based_call_get_access_token_finish (oauth2,
          &access_token, &expires_in, result, &error))
    {
      DEBUG ("Failed to get access token: %s", error->message);
      fail_auth (data);
      g_clear_error (&error);
      return;
    }

  DEBUG ("Got access token for %s:\n%s",
      tp_proxy_get_object_path (data->account),
      access_token);

  switch (empathy_sasl_channel_select_mechanism (data->channel))
    {
      case EMPATHY_SASL_MECHANISM_FACEBOOK:
        empathy_sasl_auth_facebook_async (data->channel,
            goa_oauth2_based_get_client_id (oauth2), access_token,
            auth_cb, NULL);
        break;

      case EMPATHY_SASL_MECHANISM_WLM:
        empathy_sasl_auth_wlm_async (data->channel,
            access_token,
            auth_cb, NULL);
        break;

      default:
        g_assert_not_reached ();
    }

  g_free (access_token);
}

static void
ensure_credentials_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  AuthData *data = user_data;
  GoaAccount *goa_account = (GoaAccount *) source;
  GoaOAuth2Based *oauth2;
  gint expires_in;
  GError *error = NULL;

  if (!goa_account_call_ensure_credentials_finish (goa_account, &expires_in,
      result, &error))
    {
      DEBUG ("Failed to EnsureCredentials: %s", error->message);
      fail_auth (data);
      g_clear_error (&error);
      return;
    }

  /* We support only oaut2 */
  oauth2 = goa_object_get_oauth2_based (data->goa_object);
  if (oauth2 == NULL)
    {
      DEBUG ("GoaObject does not implement oauth2");
      fail_auth (data);
      return;
    }

  DEBUG ("Goa daemon has credentials for %s, get the access token",
      tp_proxy_get_object_path (data->account));

  goa_oauth2_based_call_get_access_token (oauth2, NULL,
      got_oauth2_access_token_cb, data);

  g_object_unref (oauth2);
}

static void
start_auth (AuthData *data)
{
  EmpathyGoaAuthHandler *self = data->self;
  const GValue *id_value;
  const gchar *id;
  GList *goa_accounts, *l;
  gboolean found = FALSE;

  id_value = tp_account_get_storage_identifier (data->account);
  id = g_value_get_string (id_value);

  goa_accounts = goa_client_get_accounts (self->priv->client);
  for (l = goa_accounts; l != NULL && !found; l = l->next)
    {
      GoaObject *goa_object = l->data;
      GoaAccount *goa_account;

      goa_account = goa_object_get_account (goa_object);
      if (!tp_strdiff (goa_account_get_id (goa_account), id))
        {
          data->goa_object = g_object_ref (goa_object);

          DEBUG ("Found the GoaAccount for %s, ensure credentials",
              tp_proxy_get_object_path (data->account));

          goa_account_call_ensure_credentials (goa_account, NULL,
              ensure_credentials_cb, data);

          found = TRUE;
        }

      g_object_unref (goa_account);
    }
  g_list_free_full (goa_accounts, g_object_unref);

  if (!found)
    {
      DEBUG ("Cannot find GoaAccount");
      fail_auth (data);
    }
}

static void
client_new_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  EmpathyGoaAuthHandler *self = user_data;
  GList *l;
  GError *error = NULL;

  self->priv->client_preparing = FALSE;
  self->priv->client = goa_client_new_finish (result, &error);
  if (self->priv->client == NULL)
    {
      DEBUG ("Error getting GoaClient: %s", error->message);
      g_clear_error (&error);
    }

  /* process queued data */
  for (l = self->priv->auth_queue; l != NULL; l = l->next)
    {
      AuthData *data = l->data;

      if (self->priv->client != NULL)
        start_auth (data);
      else
        fail_auth (data);
    }

  tp_clear_pointer (&self->priv->auth_queue, g_list_free);
}

void
empathy_goa_auth_handler_start (EmpathyGoaAuthHandler *self,
    TpChannel *channel,
    TpAccount *account)
{
  AuthData *data;

  g_return_if_fail (TP_IS_CHANNEL (channel));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (empathy_goa_auth_handler_supports (self, channel, account));

  DEBUG ("Start Goa auth for account: %s",
      tp_proxy_get_object_path (account));

  data = auth_data_new (self, channel, account);

  if (self->priv->client == NULL)
    {
      /* GOA client not ready yet, queue data */
      if (!self->priv->client_preparing)
        {
          goa_client_new (NULL, client_new_cb, self);
          self->priv->client_preparing = TRUE;
        }

      self->priv->auth_queue = g_list_prepend (self->priv->auth_queue, data);
    }
  else
    {
      start_auth (data);
    }
}

gboolean
empathy_goa_auth_handler_supports (EmpathyGoaAuthHandler *self,
    TpChannel *channel,
    TpAccount *account)
{
  const gchar *provider;
  EmpathySaslMechanism mech;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);

  provider = tp_account_get_storage_provider (account);
  if (tp_strdiff (provider, EMPATHY_GOA_PROVIDER))
    return FALSE;

  mech = empathy_sasl_channel_select_mechanism (channel);
  return mech == EMPATHY_SASL_MECHANISM_FACEBOOK ||
      mech == EMPATHY_SASL_MECHANISM_WLM;
}
