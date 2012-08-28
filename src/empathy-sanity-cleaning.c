/*
 * empathy-sanity-cleaning.c
 * Code automatically called when starting a specific version of Empathy for
 * the first time doing misc cleaning.
 *
 * Copyright (C) 2012 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

#include "empathy-sanity-cleaning.h"

#include <telepathy-glib/telepathy-glib.h>

#include <libempathy/empathy-account-settings.h>
#include <libempathy/empathy-gsettings.h>

#include <libempathy-gtk/empathy-theme-manager.h>

#ifdef HAVE_UOA
#include <libempathy/empathy-pkg-kit.h>
#include <libempathy/empathy-uoa-utils.h>

#include <libaccounts-glib/ag-account-service.h>
#include <libaccounts-glib/ag-manager.h>
#include <libaccounts-glib/ag-service.h>
#endif

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-keyring.h>

/*
 * This number has to be increased each time a new task is added or modified.
 *
 * If the number stored in gsettings is lower than it, all the tasks will
 * be executed.
 */
#define SANITY_CLEANING_NUMBER 4

typedef struct
{
  TpAccountManager *am;
  GSimpleAsyncResult *result;

  gint ref_count;
} SanityCtx;

static SanityCtx *
sanity_ctx_new (TpAccountManager *am,
    GSimpleAsyncResult *result)
{
  SanityCtx *ctx = g_slice_new0 (SanityCtx);

  ctx->am = g_object_ref (am);
  ctx->result = g_object_ref (result);

  ctx->ref_count = 1;
  return ctx;
}

static SanityCtx *
sanity_ctx_ref (SanityCtx *ctx)
{
  ctx->ref_count++;

  return ctx;
}

static void
sanity_ctx_unref (SanityCtx *ctx)
{
  ctx->ref_count--;

  if (ctx->ref_count != 0)
    return;

  g_simple_async_result_complete_in_idle (ctx->result);

  g_object_unref (ctx->am);
  g_object_unref (ctx->result);

  g_slice_free (SanityCtx, ctx);
}

static void
account_update_parameters_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  TpAccount *account = TP_ACCOUNT (source);

  if (!tp_account_update_parameters_finish (account, result, NULL, &error))
    {
      DEBUG ("Failed to update parameters of account '%s': %s",
          tp_account_get_path_suffix (account), error->message);

      g_error_free (error);
      return;
    }

  tp_account_reconnect_async (account, NULL, NULL);
}

/* Make sure XMPP accounts don't have a negative priority (bgo #671452) */
static void
fix_xmpp_account_priority (TpAccountManager *am)
{
  GList *accounts, *l;

  accounts = tp_account_manager_get_valid_accounts (am);
  for (l = accounts; l != NULL; l = g_list_next (l))
    {
      TpAccount *account = l->data;
      GHashTable *params;
      gint priority;

      if (tp_strdiff (tp_account_get_protocol_name (account), "jabber"))
        continue;

      params = (GHashTable *) tp_account_get_parameters (account);
      if (params == NULL)
        continue;

      priority = tp_asv_get_int32 (params, "priority", NULL);
      if (priority >= 0)
        continue;

      DEBUG ("Resetting XMPP priority of account '%s' to 0",
          tp_account_get_path_suffix (account));

      params = tp_asv_new (
          "priority", G_TYPE_INT, 0,
          NULL);

      tp_account_update_parameters_async (account, params, NULL,
          account_update_parameters_cb, NULL);

      g_hash_table_unref (params);
    }

  g_list_free (accounts);
}

static void
set_facebook_account_fallback_server (TpAccountManager *am)
{
  GList *accounts, *l;

  accounts = tp_account_manager_get_valid_accounts (am);
  for (l = accounts; l != NULL; l = g_list_next (l))
    {
      TpAccount *account = l->data;
      GHashTable *params;
      gchar *fallback_servers[] = {
          "chat.facebook.com:443",
          NULL };

      if (tp_strdiff (tp_account_get_service (account), "facebook"))
        continue;

      params = (GHashTable *) tp_account_get_parameters (account);
      if (params == NULL)
        continue;

      if (tp_asv_get_strv (params, "fallback-servers") != NULL)
        continue;

      DEBUG ("Setting chat.facebook.com:443 as a fallback on account '%s'",
          tp_account_get_path_suffix (account));

      params = tp_asv_new (
          "fallback-servers", G_TYPE_STRV, fallback_servers,
          NULL);

      tp_account_update_parameters_async (account, params, NULL,
          account_update_parameters_cb, NULL);

      g_hash_table_unref (params);
    }

  g_list_free (accounts);
}

static void
upgrade_chat_theme_settings (void)
{
  GSettings *gsettings_chat;
  gchar *theme, *new_theme = NULL;
  const char *variant = "";

  gsettings_chat = g_settings_new (EMPATHY_PREFS_CHAT_SCHEMA);

  theme = g_settings_get_string (gsettings_chat,
      EMPATHY_PREFS_CHAT_THEME);

  if (!tp_strdiff (theme, "adium")) {
    gchar *path;

    path = g_settings_get_string (gsettings_chat,
        EMPATHY_PREFS_CHAT_ADIUM_PATH);

    new_theme = empathy_theme_manager_dup_theme_name_from_path (path);
    if (new_theme == NULL)
      {
        /* Use the Classic theme as fallback */
        new_theme = g_strdup ("Classic");
      }

    g_free (path);
  } else if (!tp_strdiff (theme, "gnome")) {
    new_theme = g_strdup ("PlanetGNOME");
  } else if (!tp_strdiff (theme, "simple")) {
    new_theme = g_strdup ("Boxes");
    variant = "Simple";
  } else if (!tp_strdiff (theme, "clean")) {
    new_theme = g_strdup ("Boxes");
    variant = "Clean";
  } else if (!tp_strdiff (theme, "blue")) {
    new_theme = g_strdup ("Boxes");
    variant = "Blue";
  } else {
    /* Assume that's an Adium theme name. The theme manager will fallback to
     * 'Classic' if it can't find it. */
    goto finally;
  }

  DEBUG ("Migrating to '%s' variant '%s'", new_theme, variant);

  g_settings_set_string (gsettings_chat,
    EMPATHY_PREFS_CHAT_THEME, new_theme);
  g_settings_set_string (gsettings_chat,
    EMPATHY_PREFS_CHAT_THEME_VARIANT, variant);

finally:
  g_free (theme);
  g_free (new_theme);
  g_object_unref (gsettings_chat);
}

#ifdef HAVE_UOA
typedef struct
{
  TpAccount *new_account;
  TpAccount *old_account;
  gboolean enabled;
} UoaMigrationData;

static UoaMigrationData *
uoa_migration_data_new (TpAccount *account)
{
  UoaMigrationData *data;

  data = g_slice_new0 (UoaMigrationData);
  data->old_account = g_object_ref (account);
  data->enabled = tp_account_is_enabled (account);

  return data;
}

static void
uoa_migration_data_free (UoaMigrationData *data)
{
  g_clear_object (&data->new_account);
  g_clear_object (&data->old_account);
  g_slice_free (UoaMigrationData, data);
}

static void
uoa_migration_done (UoaMigrationData *data)
{
  tp_account_remove_async (data->old_account, NULL, NULL);

  if (data->new_account != NULL)
    tp_account_set_enabled_async (data->new_account, data->enabled, NULL, NULL);

  uoa_migration_data_free (data);
}

static void
uoa_set_account_password_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  UoaMigrationData *data = user_data;
  GError *error = NULL;

  if (!empathy_keyring_set_account_password_finish (data->new_account, result,
          &error))
    {
      DEBUG ("Error setting old account's password on the new one: %s\n",
          error->message);
      g_clear_error (&error);
    }

  uoa_migration_done (data);
}

static void
uoa_get_account_password_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  UoaMigrationData *data = user_data;
  const gchar *password;
  GError *error = NULL;

  password = empathy_keyring_get_account_password_finish (data->old_account,
      result, &error);
  if (password == NULL)
    {
      DEBUG ("Error getting old account's password: %s\n", error->message);
      g_clear_error (&error);

      uoa_migration_done (data);
    }
  else
    {
      empathy_keyring_set_account_password_async (data->new_account, password,
          TRUE, uoa_set_account_password_cb, data);
    }
}

static void
uoa_account_created_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpAccountRequest *ar = (TpAccountRequest *) source;
  UoaMigrationData *data = user_data;
  GError *error = NULL;

  data->new_account = tp_account_request_create_account_finish (ar, result,
      &error);
  if (data->new_account == NULL)
    {
      DEBUG ("Failed to migrate account '%s' to UOA: %s",
          tp_account_get_path_suffix (data->old_account), error->message);
      g_clear_error (&error);

      uoa_migration_done (data);
    }
  else
    {
      DEBUG ("New account %s created to superseed %s",
          tp_account_get_path_suffix (data->new_account),
          tp_account_get_path_suffix (data->old_account));

      /* Migrate password as well */
      empathy_keyring_get_account_password_async (data->old_account,
          uoa_get_account_password_cb, data);
    }
}

#define DATA_SANITY_CTX "data-sanity-ctx"

static void
migrate_account_to_uoa (TpAccountManager *am,
    TpAccount *account)
{
  TpAccountRequest *ar;
  GVariant *params;
  GVariant *param;
  GVariantIter iter;
  const gchar * const *supersedes;
  UoaMigrationData *data;

  DEBUG ("Migrating account %s to UOA storage\n",
      tp_account_get_path_suffix (account));

  ar = tp_account_request_new (am,
      tp_account_get_cm_name (account),
      tp_account_get_protocol_name (account),
      tp_account_get_display_name (account));
  tp_account_request_set_storage_provider (ar, EMPATHY_UOA_PROVIDER);
  tp_account_request_set_icon_name (ar,
      tp_account_get_icon_name (account));
  tp_account_request_set_nickname (ar,
      tp_account_get_nickname (account));
  tp_account_request_set_service (ar,
      tp_account_get_service (account));

  /* Do not enable the new account until we imported the password as well */
  tp_account_request_set_enabled (ar, FALSE);

  supersedes = tp_account_get_supersedes (account);
  while (*supersedes != NULL)
    tp_account_request_add_supersedes (ar, *supersedes);
  tp_account_request_add_supersedes (ar,
      tp_proxy_get_object_path (account));

  params = tp_account_dup_parameters_vardict (account);
  g_variant_iter_init (&iter, params);
  while ((param = g_variant_iter_next_value (&iter)))
    {
      GVariant *k, *v;
      const gchar *key;

      k = g_variant_get_child_value (param, 0);
      key = g_variant_get_string (k, NULL);
      v = g_variant_get_child_value (param, 1);

      tp_account_request_set_parameter (ar, key,
          g_variant_get_variant (v));

      g_variant_unref (k);
      g_variant_unref (v);
    }

  data = uoa_migration_data_new (account);
  tp_account_set_enabled_async (account, FALSE, NULL, NULL);
  tp_account_request_create_account_async (ar, uoa_account_created_cb,
      data);

  g_object_set_data (G_OBJECT (account), DATA_SANITY_CTX, NULL);

  g_variant_unref (params);
  g_object_unref (ar);
}

static void
uoa_account_remove_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpAccount *account = TP_ACCOUNT (source);
  GError *error = NULL;

  if (!tp_account_remove_finish (account, result, &error))
    {
      DEBUG ("Failed to remove account '%s': %s",
          tp_account_get_path_suffix (account), error->message);
      g_error_free (error);
    }

  g_object_set_data (G_OBJECT (account), DATA_SANITY_CTX, NULL);
}

static void
uoa_plugin_install_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpAccount *account = user_data;
  GError *error = NULL;
  TpAccountManager *am;

  if (!empathy_pkg_kit_install_packages_finish (result, &error))
    {
      DEBUG ("Failed to install plugin for account '%s' (%s); remove it",
          tp_account_get_path_suffix (account), error->message);

      g_error_free (error);

      tp_account_remove_async (account, uoa_account_remove_cb, NULL);
      goto out;
    }

  DEBUG ("Plugin for account '%s' has been installed; migrate account",
      tp_account_get_path_suffix (account));

  am = tp_account_manager_dup ();
  migrate_account_to_uoa (am, account);
  g_object_unref (am);

out:
  g_object_unref (account);
}

static gchar *
dup_plugin_name_for_protocol (const gchar *protocol)
{
  if (!tp_strdiff (protocol, "local-xmpp"))
    return g_strdup ("account-plugin-salut");

  return g_strdup_printf ("account-plugin-%s", protocol);
}

static gboolean
uoa_plugin_installed (AgManager *manager,
    TpAccount *account)
{
  AgAccount *ag_account;
  const gchar *protocol;
  GList *l;

  protocol = tp_account_get_protocol_name (account);
  ag_account = ag_manager_create_account (manager, protocol);

  l = ag_account_list_services_by_type (ag_account, EMPATHY_UOA_SERVICE_TYPE);
  if (l == NULL)
    {
      const gchar *packages[2];
      gchar *pkg;

      pkg = dup_plugin_name_for_protocol (protocol);

      DEBUG ("%s is not installed; try to install it", pkg);

      packages[0] = pkg;
      packages[1] = NULL;

      empathy_pkg_kit_install_packages_async (0, packages, NULL,
          NULL, uoa_plugin_install_cb, g_object_ref (account));

      g_free (pkg);
      g_object_unref (ag_account);
      return FALSE;
    }

  ag_service_list_free (l);

  g_object_unref (ag_account);
  return TRUE;
}

static void
migrate_accounts_to_uoa (SanityCtx *ctx)
{
  GList *accounts, *l;
  AgManager *manager;

  DEBUG ("Start migrating accounts to UOA");

  manager = empathy_uoa_manager_dup ();

  accounts = tp_account_manager_get_valid_accounts (ctx->am);
  for (l = accounts; l != NULL; l = g_list_next (l))
    {
      TpAccount *account = l->data;

      /* If account is already in a specific storage (like UOA or GOA),
       * don't migrate it.
       * Note that we cannot migrate GOA accounts anyway, since we can't delete
       * them it would create duplicated accounts. */
      if (!tp_str_empty (tp_account_get_storage_provider (account)))
        continue;

      g_object_set_data_full (G_OBJECT (account), DATA_SANITY_CTX,
          sanity_ctx_ref (ctx), (GDestroyNotify) sanity_ctx_unref);

      /* Try to install the plugin if it's missing */
      if (!uoa_plugin_installed (manager, account))
        continue;

      migrate_account_to_uoa (ctx->am, account);
    }

  g_object_unref (manager);
}
#endif

static void
run_sanity_cleaning_tasks (SanityCtx *ctx)
{
  DEBUG ("Starting sanity cleaning tasks");

  fix_xmpp_account_priority (ctx->am);
  set_facebook_account_fallback_server (ctx->am);
  upgrade_chat_theme_settings ();
#ifdef HAVE_UOA
  migrate_accounts_to_uoa (ctx);
#endif
}

static void
am_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  TpAccountManager *am = TP_ACCOUNT_MANAGER (source);
  SanityCtx *ctx = user_data;

  if (!tp_proxy_prepare_finish (am, result, &error))
    {
      DEBUG ("Failed to prepare account manager: %s", error->message);
      g_simple_async_result_take_error (ctx->result, error);
      goto out;
    }

  run_sanity_cleaning_tasks (ctx);

out:
  sanity_ctx_unref (ctx);
}

void
empathy_sanity_checking_run_async (GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSettings *settings;
  guint number;
  TpAccountManager *am;
  GSimpleAsyncResult *result;
  SanityCtx *ctx;

  result = g_simple_async_result_new (NULL, callback, user_data,
      empathy_sanity_checking_run_async);

  settings = g_settings_new (EMPATHY_PREFS_SCHEMA);
  number = g_settings_get_uint (settings, EMPATHY_PREFS_SANITY_CLEANING_NUMBER);

  if (number == SANITY_CLEANING_NUMBER)
    {
      g_simple_async_result_complete_in_idle (result);
      goto out;
    }

  am = tp_account_manager_dup ();

  ctx = sanity_ctx_new (am, result);
  tp_proxy_prepare_async (am, NULL, am_prepare_cb, ctx);

  g_settings_set_uint (settings, EMPATHY_PREFS_SANITY_CLEANING_NUMBER,
      SANITY_CLEANING_NUMBER);

  g_object_unref (am);

out:
  g_object_unref (settings);
  g_object_unref (result);
}

gboolean
empathy_sanity_checking_run_finish (GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
        empathy_sanity_checking_run_async), FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
        error))
    return FALSE;

  return TRUE;
}
