/*
 * Copyright © 2012 Collabora Ltd.
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
#include "mcp-account-manager-uoa.h"

#include <telepathy-glib/telepathy-glib.h>

#include <libaccounts-glib/ag-account.h>
#include <libaccounts-glib/ag-account-service.h>
#include <libaccounts-glib/ag-manager.h>
#include <libaccounts-glib/ag-service.h>

#include <string.h>
#include <ctype.h>

#define PLUGIN_NAME "uoa"
#define PLUGIN_PRIORITY (MCP_ACCOUNT_STORAGE_PLUGIN_PRIO_KEYRING + 10)
#define PLUGIN_DESCRIPTION "Provide Telepathy Accounts from UOA via libaccounts-glib"
#define PLUGIN_PROVIDER EMPATHY_UOA_PROVIDER

#define DEBUG g_debug

#define SERVICE_TYPE "IM"
#define KEY_PREFIX "telepathy/"
#define KEY_ACCOUNT_NAME "mc-account-name"
#define KEY_READONLY_PARAMS "mc-readonly-params"

static void account_storage_iface_init (McpAccountStorageIface *iface);

G_DEFINE_TYPE_WITH_CODE (McpAccountManagerUoa, mcp_account_manager_uoa,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (MCP_TYPE_ACCOUNT_STORAGE,
        account_storage_iface_init));

struct _McpAccountManagerUoaPrivate
{
  McpAccountManager *am;

  AgManager *manager;

  /* alloc'ed string -> ref'ed AgAccountService
   * The key is the account_name, an MC unique identifier.
   * Note: There could be multiple services in this table having the same
   * AgAccount, even if unlikely. */
  GHashTable *accounts;

  /* Queue of owned DelayedSignalData */
  GQueue *pending_signals;

  gboolean loaded;
  gboolean ready;
};

typedef enum {
  DELAYED_CREATE,
  DELAYED_DELETE,
} DelayedSignal;

typedef struct {
  DelayedSignal signal;
  AgAccountId account_id;
} DelayedSignalData;


static gchar *
_service_dup_tp_value (AgAccountService *service,
    const gchar *key)
{
  gchar *real_key = g_strdup_printf (KEY_PREFIX "%s", key);
  GValue value = { 0, };
  gchar *ret;

  g_value_init (&value, G_TYPE_STRING);
  ag_account_service_get_value (service, real_key, &value);
  ret = g_value_dup_string (&value);
  g_value_unset (&value);

  return ret;
}

static void
_service_set_tp_value (AgAccountService *service,
    const gchar *key,
    const gchar *value)
{
  gchar *real_key = g_strdup_printf (KEY_PREFIX "%s", key);

  if (value != NULL)
    {
      GValue gvalue = { 0, };

      g_value_init (&gvalue, G_TYPE_STRING);
      g_value_set_string (&gvalue, value);
      ag_account_service_set_value (service, real_key, &gvalue);
      g_value_unset (&gvalue);
      g_free (real_key);
    }
  else
    {
      ag_account_service_set_value (service, real_key, NULL);
    }
}

/* Returns NULL if the account never has been imported into MC before */
static gchar *
_service_dup_tp_account_name (AgAccountService *service)
{
  return _service_dup_tp_value (service, KEY_ACCOUNT_NAME);
}

static void
_service_set_tp_account_name (AgAccountService *service,
    const gchar *account_name)
{
  _service_set_tp_value (service, KEY_ACCOUNT_NAME, account_name);
}

static void
_service_enabled_cb (AgAccountService *service,
    gboolean enabled,
    McpAccountManagerUoa *self)
{
  gchar *account_name = _service_dup_tp_account_name (service);

  if (!self->priv->ready || account_name == NULL)
    return;

  DEBUG ("UOA account %s toggled: %s", account_name,
      enabled ? "enabled" : "disabled");

  g_signal_emit_by_name (self, "toggled", account_name, enabled);

  g_free (account_name);
}

static void
_service_changed_cb (AgAccountService *service,
    McpAccountManagerUoa *self)
{
  gchar *account_name = _service_dup_tp_account_name (service);

  if (!self->priv->ready || account_name == NULL)
    return;

  DEBUG ("UOA account %s changed", account_name);

  /* FIXME: Could use ag_account_service_get_changed_fields()
   * and emit "altered-one" */
  g_signal_emit_by_name (self, "altered", account_name);

  g_free (account_name);
}

static void
_account_stored_cb (AgAccount *account,
    const GError *error,
    gpointer user_data)
{
  if (error != NULL)
    {
      DEBUG ("Error storing UOA account '%s': %s",
          ag_account_get_display_name (account),
          error->message);
    }
}

static gboolean
_add_service (McpAccountManagerUoa *self,
    AgAccountService *service,
    const gchar *account_name)
{
  DEBUG ("UOA account %s added", account_name);

  if (g_hash_table_contains (self->priv->accounts, account_name))
    {
      DEBUG ("Already exists, ignoring");
      return FALSE;
    }

  g_hash_table_insert (self->priv->accounts,
      g_strdup (account_name),
      g_object_ref (service));

  g_signal_connect (service, "enabled",
      G_CALLBACK (_service_enabled_cb), self);
  g_signal_connect (service, "changed",
      G_CALLBACK (_service_changed_cb), self);

  return TRUE;
}

static void
_account_created_cb (AgManager *manager,
    AgAccountId id,
    McpAccountManagerUoa *self)
{
  AgAccount *account;
  GList *l;

  if (!self->priv->ready)
    {
      DelayedSignalData *data = g_slice_new0 (DelayedSignalData);

      data->signal = DELAYED_CREATE;
      data->account_id = id;

      g_queue_push_tail (self->priv->pending_signals, data);
      return;
    }

  account = ag_manager_get_account (self->priv->manager, id);

  l = ag_account_list_services_by_type (account, SERVICE_TYPE);
  while (l != NULL)
    {
      AgAccountService *service = ag_account_service_new (account, l->data);
      gchar *account_name = _service_dup_tp_account_name (service);

      /* If this is the first time we see this service, we have to generate an
       * account_name for it. */
      if (account_name == NULL)
        {
          gchar *cm_name = NULL;
          gchar *protocol_name = NULL;
          gchar *account_param = NULL;

          cm_name = _service_dup_tp_value (service, "manager");
          protocol_name = _service_dup_tp_value (service, "protocol");
          account_param = _service_dup_tp_value (service, "param-account");

          if (!tp_str_empty (cm_name) &&
              !tp_str_empty (protocol_name) &&
              !tp_str_empty (account_param))
            {
              GHashTable *params;

              params = tp_asv_new (
                  "account", G_TYPE_STRING, account_param,
                  NULL);

              account_name = mcp_account_manager_get_unique_name (self->priv->am,
                  cm_name, protocol_name, params);
              _service_set_tp_account_name (service, account_name);

              ag_account_store (account, _account_stored_cb, self);

              g_hash_table_unref (params);
            }

          g_free (cm_name);
          g_free (protocol_name);
          g_free (account_param);
        }

      if (account_name != NULL)
        {
          if (_add_service (self, service, account_name))
            g_signal_emit_by_name (self, "created", account_name);
        }

      g_free (account_name);
      g_object_unref (service);
      ag_service_unref (l->data);
      l = g_list_delete_link (l, l);
    }

  g_object_unref (account);
}

static void
_account_deleted_cb (AgManager *manager,
    AgAccountId id,
    McpAccountManagerUoa *self)
{
  GHashTableIter iter;
  gpointer value;

  if (!self->priv->ready)
    {
      DelayedSignalData *data = g_slice_new0 (DelayedSignalData);

      data->signal = DELAYED_DELETE;
      data->account_id = id;

      g_queue_push_tail (self->priv->pending_signals, data);
      return;
    }

  g_hash_table_iter_init (&iter, self->priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      AgAccountService *service = value;
      AgAccount *account = ag_account_service_get_account (service);
      gchar *account_name;

      if (account->id != id)
        continue;

      account_name = _service_dup_tp_account_name (service);
      if (account_name == NULL)
        continue;

      DEBUG ("UOA account %s deleted", account_name);

      g_hash_table_iter_remove (&iter);
      g_signal_emit_by_name (self, "deleted", account_name);

      g_free (account_name);
    }
}

static void
mcp_account_manager_uoa_dispose (GObject *object)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) object;

  tp_clear_object (&self->priv->am);
  tp_clear_object (&self->priv->manager);
  tp_clear_pointer (&self->priv->accounts, g_hash_table_unref);

  G_OBJECT_CLASS (mcp_account_manager_uoa_parent_class)->dispose (object);
}

static void
mcp_account_manager_uoa_init (McpAccountManagerUoa *self)
{
  DEBUG ("UOA MC plugin initialised");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MCP_TYPE_ACCOUNT_MANAGER_UOA, McpAccountManagerUoaPrivate);

  self->priv->accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);
  self->priv->pending_signals = g_queue_new ();

  self->priv->manager = ag_manager_new_for_service_type (SERVICE_TYPE);

  g_signal_connect (self->priv->manager, "account-created",
      G_CALLBACK (_account_created_cb), self);
  g_signal_connect (self->priv->manager, "account-deleted",
      G_CALLBACK (_account_deleted_cb), self);
}

static void
mcp_account_manager_uoa_class_init (McpAccountManagerUoaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = mcp_account_manager_uoa_dispose;

  g_type_class_add_private (gobject_class,
      sizeof (McpAccountManagerUoaPrivate));
}

static void
_ensure_loaded (McpAccountManagerUoa *self)
{
  GList *services;

  if (self->priv->loaded)
    return;

  self->priv->loaded = TRUE;

  g_assert (!self->priv->ready);

  services = ag_manager_get_account_services (self->priv->manager);
  while (services != NULL)
    {
      AgAccountService *service = services->data;
      AgAccount *account = ag_account_service_get_account (service);
      gchar *account_name = _service_dup_tp_account_name (service);

      if (account_name != NULL)
        {
          /* This service was already known, we can add it now */
          _add_service (self, service, account_name);
          g_free (account_name);
        }
      else
        {
          DelayedSignalData *data = g_slice_new0 (DelayedSignalData);

          /* This service was created while MC was not running, delay its
           * creation until MC is ready */
          data->signal = DELAYED_CREATE;
          data->account_id = account->id;

          g_queue_push_tail (self->priv->pending_signals, data);
        }

      g_object_unref (services->data);
      services = g_list_delete_link (services, services);
    }
}

static GList *
account_manager_uoa_list (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  GList *accounts = NULL;
  GHashTableIter iter;
  gpointer key;

  DEBUG (G_STRFUNC);

  _ensure_loaded (self);

  g_hash_table_iter_init (&iter, self->priv->accounts);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    accounts = g_list_prepend (accounts, g_strdup (key));

  return accounts;
}

static gboolean
account_manager_uoa_get (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *account_name,
    const gchar *key)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  AgAccountService *service;
  AgAccount *account;
  AgService *s;
  gboolean handled = FALSE;

  service = g_hash_table_lookup (self->priv->accounts, account_name);
  if (service == NULL)
    return FALSE;

  DEBUG ("%s: %s, %s", G_STRFUNC, account_name, key);

  account = ag_account_service_get_account (service);
  s = ag_account_service_get_service (service);

  /* NULL key means we want all settings */
  if (key == NULL)
    {
      AgAccountSettingIter iter;
      const gchar *k;
      const GValue *v;

      ag_account_service_settings_iter_init (service, &iter, KEY_PREFIX);
      while (ag_account_service_settings_iter_next (&iter, &k, &v))
        {
          if (!G_VALUE_HOLDS_STRING (v))
            continue;

          mcp_account_manager_set_value (am, account_name,
              k, g_value_get_string (v));
        }
    }

  /* Some special keys that are not stored in setting */
  if (key == NULL || !tp_strdiff (key, "Enabled"))
    {
      mcp_account_manager_set_value (am, account_name, "Enabled",
          ag_account_service_get_enabled (service) ? "true" : "false");
      handled = TRUE;
    }

  if (key == NULL || !tp_strdiff (key, "DisplayName"))
    {
      mcp_account_manager_set_value (am, account_name, "DisplayName",
          ag_account_get_display_name (account));
      handled = TRUE;
    }

  if (key == NULL || !tp_strdiff (key, "Service"))
    {
      mcp_account_manager_set_value (am, account_name, "Service",
          ag_account_get_provider_name (account));
      handled = TRUE;
    }

  if (key == NULL || !tp_strdiff (key, "Icon"))
    {
      mcp_account_manager_set_value (am, account_name, "Icon",
          ag_service_get_icon_name (s));
      handled = TRUE;
    }

  /* If it was none of the above, then just lookup in service' settings */
  if (!handled)
    {
      gchar *value = _service_dup_tp_value (service, key);

      mcp_account_manager_set_value (am, account_name, key, value);
      g_free (value);
    }

  return TRUE;
}

static gboolean
account_manager_uoa_set (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *account_name,
    const gchar *key,
    const gchar *val)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  AgAccountService *service;
  AgAccount *account;

  service = g_hash_table_lookup (self->priv->accounts, account_name);
  if (service == NULL)
    return FALSE;

  account = ag_account_service_get_account (service);

  DEBUG ("%s: %s, %s, %s", G_STRFUNC, account_name, key, val);

  if (!tp_strdiff (key, "Enabled"))
    {
      ag_account_set_enabled (account, !tp_strdiff (val, "true"));
    }
  else if (!tp_strdiff (key, "DisplayName"))
    {
      ag_account_set_display_name (account, val);
    }
  else
    {
      _service_set_tp_value (service, key, val);
    }

  return TRUE;
}

static gchar *
account_manager_uoa_create (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *cm_name,
    const gchar *protocol_name,
    GHashTable *params,
    GError **error)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  gchar *account_name;
  AgAccount *account;
  AgAccountService *service;
  GList *l;

  if (!self->priv->ready)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Cannot create account before being ready");
      return NULL;
    }

  DEBUG (G_STRFUNC);

  /* Create a new AgAccountService and keep it internally. This won't save it
   * into persistent storage until account_manager_uoa_commit() is called.
   * We assume there is only one IM service */
  account = ag_manager_create_account (self->priv->manager, protocol_name);
  l = ag_account_list_services_by_type (account, SERVICE_TYPE);
  if (l == NULL)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Cannot create a %s service for %s provider",
          SERVICE_TYPE, protocol_name);
      g_object_unref (account);
      return NULL;
    }
  service = ag_account_service_new (account, l->data);
  ag_service_list_free (l);
  g_object_unref (account);

  account_name = mcp_account_manager_get_unique_name (self->priv->am,
      cm_name, protocol_name, params);
  _service_set_tp_account_name (service, account_name);
  g_assert (_add_service (self, service, account_name));

  /* MC will set all params on the account and commit */

  return account_name;
}

static gboolean
account_manager_uoa_delete (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *account_name,
    const gchar *key)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  AgAccountService *service;
  AgAccount *account;

  service = g_hash_table_lookup (self->priv->accounts, account_name);
  if (service == NULL)
    return FALSE;

  account = ag_account_service_get_account (service);

  DEBUG ("%s: %s, %s", G_STRFUNC, account_name, key);

  if (key == NULL)
    {
      ag_account_delete (account);
      g_hash_table_remove (self->priv->accounts, account_name);
    }
  else
    {
      _service_set_tp_value (service, key, NULL);
    }

  return TRUE;
}

static gboolean
account_manager_uoa_commit (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  GHashTableIter iter;
  gpointer value;

  DEBUG (G_STRFUNC);

  g_hash_table_iter_init (&iter, self->priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      AgAccountService *service = value;
      AgAccount *account = ag_account_service_get_account (service);

      ag_account_store (account, _account_stored_cb, self);
    }

  return TRUE;
}

static void
account_manager_uoa_ready (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  DelayedSignalData *data;

  if (self->priv->ready)
    return;

  DEBUG (G_STRFUNC);

  self->priv->ready = TRUE;
  self->priv->am = g_object_ref (G_OBJECT (am));

  while ((data = g_queue_pop_head (self->priv->pending_signals)) != NULL)
    {
      switch (data->signal)
        {
          case DELAYED_CREATE:
            _account_created_cb (self->priv->manager, data->account_id, self);
            break;
          case DELAYED_DELETE:
            _account_deleted_cb (self->priv->manager, data->account_id, self);
            break;
          default:
            g_assert_not_reached ();
        }

      g_slice_free (DelayedSignalData, data);
    }

  g_queue_free (self->priv->pending_signals);
  self->priv->pending_signals = NULL;
}

static void
account_manager_uoa_get_identifier (const McpAccountStorage *storage,
    const gchar *account_name,
    GValue *identifier)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  AgAccountService *service;
  AgAccount *account;

  service = g_hash_table_lookup (self->priv->accounts, account_name);
  if (service == NULL)
    return;

  account = ag_account_service_get_account (service);

  g_value_init (identifier, G_TYPE_UINT);
  g_value_set_uint (identifier, account->id);
}

static guint
account_manager_uoa_get_restrictions (const McpAccountStorage *storage,
    const gchar *account_name)
{
  McpAccountManagerUoa *self = (McpAccountManagerUoa *) storage;
  AgAccountService *service;
  guint restrictions = TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_SERVICE;
  GValue value = G_VALUE_INIT;

  /* If we don't know this account, we cannot do anything */
  service = g_hash_table_lookup (self->priv->accounts, account_name);
  if (service == NULL)
    return G_MAXUINT;

  g_value_init (&value, G_TYPE_BOOLEAN);
  ag_account_service_get_value (service,
      KEY_PREFIX KEY_READONLY_PARAMS, &value);

  if (g_value_get_boolean (&value))
    restrictions |= TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS;

  g_value_unset (&value);

  /* FIXME: We can't set Icon either, but there is no flag for that */
  return restrictions;
}

static void
account_storage_iface_init (McpAccountStorageIface *iface)
{
  mcp_account_storage_iface_set_name (iface, PLUGIN_NAME);
  mcp_account_storage_iface_set_desc (iface, PLUGIN_DESCRIPTION);
  mcp_account_storage_iface_set_priority (iface, PLUGIN_PRIORITY);
  mcp_account_storage_iface_set_provider (iface, PLUGIN_PROVIDER);

#define IMPLEMENT(x) mcp_account_storage_iface_implement_##x(iface, \
    account_manager_uoa_##x)
  IMPLEMENT (get);
  IMPLEMENT (list);
  IMPLEMENT (set);
  IMPLEMENT (create);
  IMPLEMENT (delete);
  IMPLEMENT (commit);
  IMPLEMENT (ready);
  IMPLEMENT (get_identifier);
  IMPLEMENT (get_restrictions);
#undef IMPLEMENT
}

McpAccountManagerUoa *
mcp_account_manager_uoa_new (void)
{
  return g_object_new (MCP_TYPE_ACCOUNT_MANAGER_UOA, NULL);
}
