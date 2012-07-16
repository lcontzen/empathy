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

#include <string.h>
#include <ctype.h>

#define PLUGIN_NAME "uoa"
#define PLUGIN_PRIORITY (MCP_ACCOUNT_STORAGE_PLUGIN_PRIO_KEYRING + 10)
#define PLUGIN_DESCRIPTION "Provide Telepathy Accounts from UOA via libaccounts-glib"
#define PLUGIN_PROVIDER EMPATHY_UOA_PROVIDER

#define DEBUG g_debug

static void account_storage_iface_init (McpAccountStorageIface *iface);

G_DEFINE_TYPE_WITH_CODE (McpAccountManagerUoa, mcp_account_manager_uoa,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (MCP_TYPE_ACCOUNT_STORAGE,
        account_storage_iface_init));

struct _McpAccountManagerUoaPrivate
{
};

static void
mcp_account_manager_uoa_dispose (GObject *object)
{
  G_OBJECT_CLASS (mcp_account_manager_uoa_parent_class)->dispose (object);
}

static void
mcp_account_manager_uoa_init (McpAccountManagerUoa *self)
{
  DEBUG ("UOA MC plugin initialised");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MCP_TYPE_ACCOUNT_MANAGER_UOA, McpAccountManagerUoaPrivate);
}

static void
mcp_account_manager_uoa_class_init (McpAccountManagerUoaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = mcp_account_manager_uoa_dispose;

  g_type_class_add_private (gobject_class,
      sizeof (McpAccountManagerUoaPrivate));
}

static GList *
account_manager_uoa_list (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
  return NULL;
}

static gboolean
account_manager_uoa_get (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *acc,
    const gchar *key)
{
  return FALSE;
}

static gboolean
account_manager_uoa_set (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *acc,
    const gchar *key,
    const gchar *val)
{
  return FALSE;
}

static gboolean
account_manager_uoa_delete (const McpAccountStorage *storage,
    const McpAccountManager *am,
    const gchar *acc,
    const gchar *key)
{
  return FALSE;
}

static gboolean
account_manager_uoa_commit (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
  return FALSE;
}

static void
account_manager_uoa_ready (const McpAccountStorage *storage,
    const McpAccountManager *am)
{
}

static void
account_manager_uoa_get_identifier (const McpAccountStorage *storage,
    const gchar *acc,
    GValue *identifier)
{
}

static gchar *
account_manager_uoa_create_account (const McpAccountStorage *storage,
    const gchar *cm_name,
    const gchar *protocol_name,
    GHashTable *params)
{
  return NULL;
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
  IMPLEMENT (delete);
  IMPLEMENT (commit);
  IMPLEMENT (ready);
  IMPLEMENT (get_identifier);
  IMPLEMENT (create_account);
#undef IMPLEMENT
}

McpAccountManagerUoa *
mcp_account_manager_uoa_new (void)
{
  return g_object_new (MCP_TYPE_ACCOUNT_MANAGER_UOA, NULL);
}
