/*
 * Copyright (C) 2010 Collabora Ltd.
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

#include "empathy-keyring.h"

#include <string.h>

#include <libsecret/secret.h>

#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

static const SecretSchema account_keyring_schema =
  { "org.gnome.Empathy.Account", SECRET_SCHEMA_DONT_MATCH_NAME,
    { { "account-id", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "param-name", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { NULL } } };

static const SecretSchema room_keyring_schema =
  { "org.gnome.Empathy.Room", SECRET_SCHEMA_DONT_MATCH_NAME,
    { { "account-id", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "room-id", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { NULL } } };

gboolean
empathy_keyring_is_available (void)
{
  return TRUE;
}

/* get */

static void
lookup_item_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;
  gchar *password;

  password = secret_password_lookup_finish (result, &error);
  if (error != NULL)
    {
      g_simple_async_result_set_error (simple, TP_ERROR,
          TP_ERROR_DOES_NOT_EXIST, "%s", error->message);
      g_clear_error (&error);
      goto out;
    }

  if (password == NULL)
    {
      g_simple_async_result_set_error (simple, TP_ERROR,
          TP_ERROR_DOES_NOT_EXIST, _("Password not found"));
      goto out;
    }

  g_simple_async_result_set_op_res_gpointer (simple, password,
      (GDestroyNotify) secret_password_free);

out:
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

void
empathy_keyring_get_account_password_async (TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  const gchar *account_id;

  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (callback != NULL);

  simple = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, empathy_keyring_get_account_password_async);

  account_id = tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  DEBUG ("Trying to get password for: %s", account_id);

  secret_password_lookup (&account_keyring_schema, NULL,
          lookup_item_cb, simple,
          "account-id", account_id,
          "param-name", "password",
          NULL);
}

void
empathy_keyring_get_room_password_async (TpAccount *account,
    const gchar *id,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  const gchar *account_id;

  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (id != NULL);
  g_return_if_fail (callback != NULL);

  simple = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, empathy_keyring_get_room_password_async);

  account_id = tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  DEBUG ("Trying to get password for room '%s' on account '%s'",
      id, account_id);

  secret_password_lookup (&room_keyring_schema, NULL,
          lookup_item_cb, simple,
          "account-id", account_id,
          "room-id", id,
          NULL);
}

const gchar *
empathy_keyring_get_account_password_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_return_pointer (account,
      empathy_keyring_get_account_password_async);
}

const gchar *
empathy_keyring_get_room_password_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_return_pointer (account,
      empathy_keyring_get_room_password_async);
}

/* set */

static void
store_password_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;

  if (!secret_password_store_finish (result, &error))
    {
      g_simple_async_result_set_error (simple, TP_ERROR,
          TP_ERROR_DOES_NOT_EXIST, error->message);
      g_error_free (error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

void
empathy_keyring_set_account_password_async (TpAccount *account,
    const gchar *password,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  const gchar *account_id;
  gchar *name;

  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (password != NULL);

  simple = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, empathy_keyring_set_account_password_async);

  account_id = tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  DEBUG ("Remembering password for %s", account_id);

  name = g_strdup_printf (_("IM account password for %s (%s)"),
      tp_account_get_display_name (account), account_id);

  secret_password_store (&account_keyring_schema, NULL, name, password,
      NULL, store_password_cb, simple,
      "account-id", account_id,
      "param-name", "password",
      NULL);

  g_free (name);
}

void
empathy_keyring_set_room_password_async (TpAccount *account,
    const gchar *id,
    const gchar *password,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  const gchar *account_id;
  gchar *name;

  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (id != NULL);
  g_return_if_fail (password != NULL);

  simple = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, empathy_keyring_set_room_password_async);

  account_id = tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  DEBUG ("Remembering password for room '%s' on account '%s'", id, account_id);

  name = g_strdup_printf (_("Password for chatroom '%s' on account %s (%s)"),
      id, tp_account_get_display_name (account), account_id);

  secret_password_store (&room_keyring_schema, NULL, name, password,
      NULL, store_password_cb, simple,
      "account-id", account_id,
      "room-id", id,
      NULL);

  g_free (name);
}

gboolean
empathy_keyring_set_account_password_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_void (account, empathy_keyring_set_account_password_async);
}

gboolean
empathy_keyring_set_room_password_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_void (account, empathy_keyring_set_room_password_async);
}

/* delete */

static void
items_delete_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error = NULL;

  if (!secret_password_clear_finish (result, &error))
    {
      g_simple_async_result_set_error (simple, TP_ERROR,
              TP_ERROR_DOES_NOT_EXIST, "%s", error->message);
      g_error_free (error);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

void
empathy_keyring_delete_account_password_async (TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  const gchar *account_id;

  g_return_if_fail (TP_IS_ACCOUNT (account));

  simple = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, empathy_keyring_delete_account_password_async);

  account_id = tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  secret_password_clear (&account_keyring_schema, NULL,
          items_delete_cb, simple,
          "account-id", account_id,
          "param-name", "password",
          NULL);
}

gboolean
empathy_keyring_delete_account_password_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_void (account, empathy_keyring_delete_account_password_async);
}
