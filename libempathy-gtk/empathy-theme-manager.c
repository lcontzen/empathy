/*
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2008-2012 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>
#include <telepathy-glib/dbus.h>
#include <gtk/gtk.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-gsettings.h>
#include <libempathy/empathy-utils.h>

#include "empathy-theme-manager.h"
#include "empathy-chat-view.h"
#include "empathy-chat-text-view.h"
#include "empathy-theme-adium.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

struct _EmpathyThemeManagerPriv
{
  GSettings   *gsettings_chat;
  guint        emit_changed_idle;
  gboolean     in_constructor;

  EmpathyAdiumData *adium_data;
  gchar *adium_variant;
  /* list of weakref to EmpathyThemeAdium objects */
  GList *adium_views;
};

enum
{
  THEME_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EmpathyThemeManager, empathy_theme_manager, G_TYPE_OBJECT);

static gboolean
theme_manager_emit_changed_idle_cb (gpointer manager)
{
  EmpathyThemeManager *self = manager;
  const gchar *adium_path = NULL;

  if (self->priv->adium_data)
    adium_path = empathy_adium_data_get_path (self->priv->adium_data);

  DEBUG ("Emit theme-changed with: adium_path='%s' "
      "adium_variant='%s'", adium_path, self->priv->adium_variant);

  g_signal_emit (self, signals[THEME_CHANGED], 0, NULL);
  self->priv->emit_changed_idle = 0;

  return FALSE;
}

static void
theme_manager_emit_changed (EmpathyThemeManager *self)
{
  /* We emit the signal in idle callback to be sure we emit it only once
   * in the case both the name and adium_path changed */
  if (self->priv->emit_changed_idle == 0 && !self->priv->in_constructor)
    {
      self->priv->emit_changed_idle = g_idle_add (
        theme_manager_emit_changed_idle_cb, self);
    }
}

static void
theme_manager_view_weak_notify_cb (gpointer data,
    GObject *where_the_object_was)
{
  GList **list = data;

  *list = g_list_remove (*list, where_the_object_was);
}

static void
clear_list_of_views (GList **views)
{
  while (*views)
    {
      g_object_weak_unref ((*views)->data,
               theme_manager_view_weak_notify_cb,
               views);

      *views = g_list_delete_link (*views, *views);
    }
}

static EmpathyThemeAdium *
theme_manager_create_adium_view (EmpathyThemeManager *self)
{
  EmpathyThemeAdium *theme;

  theme = empathy_theme_adium_new (self->priv->adium_data, self->priv->adium_variant);

  self->priv->adium_views = g_list_prepend (self->priv->adium_views, theme);

  g_object_weak_ref (G_OBJECT (theme),
         theme_manager_view_weak_notify_cb,
         &self->priv->adium_views);

  return theme;
}

static void
theme_manager_notify_theme_cb (GSettings *gsettings_chat,
    const gchar *key,
    gpointer user_data)
{
  EmpathyThemeManager *self = EMPATHY_THEME_MANAGER (user_data);
  gchar *theme, *path;

  theme = g_settings_get_string (gsettings_chat, key);

  if (empathy_theme_manager_find_theme (theme) != NULL)
    {
      path = empathy_theme_manager_find_theme (theme);
      g_free (theme);
    }
  else
    {
      g_warning ("Can't find theme: %s; fallback to 'Classic'",
          theme);

      g_free (theme);

      path = empathy_theme_manager_find_theme ("Classic");
      if (path == NULL)
        g_critical ("Can't find 'Classic theme");
    }

  /* Load new theme data, we can stop tracking existing views since we
   * won't be able to change them live anymore */
  clear_list_of_views (&self->priv->adium_views);
  tp_clear_pointer (&self->priv->adium_data, empathy_adium_data_unref);
  self->priv->adium_data = empathy_adium_data_new (path);

  theme_manager_emit_changed (self);

  g_free (path);
}

static void
theme_manager_notify_adium_variant_cb (GSettings *gsettings_chat,
    const gchar *key,
    gpointer user_data)
{
  EmpathyThemeManager *self = EMPATHY_THEME_MANAGER (user_data);
  gchar *new_variant;
  GList *l;

  new_variant = g_settings_get_string (gsettings_chat, key);
  if (!tp_strdiff (self->priv->adium_variant, new_variant))
    {
      g_free (new_variant);
      return;
    }

  g_free (self->priv->adium_variant);
  self->priv->adium_variant = new_variant;

  for (l = self->priv->adium_views; l; l = l->next)
    {
      empathy_theme_adium_set_variant (EMPATHY_THEME_ADIUM (l->data),
        self->priv->adium_variant);
    }
}

EmpathyChatView *
empathy_theme_manager_create_view (EmpathyThemeManager *self)
{
  g_return_val_if_fail (EMPATHY_IS_THEME_MANAGER (self), NULL);

  if (self->priv->adium_data != NULL)
    return EMPATHY_CHAT_VIEW (theme_manager_create_adium_view (self));

  g_return_val_if_reached (NULL);
}

static void
theme_manager_finalize (GObject *object)
{
  EmpathyThemeManager *self = (EmpathyThemeManager *) object;

  g_object_unref (self->priv->gsettings_chat);

  if (self->priv->emit_changed_idle != 0)
    g_source_remove (self->priv->emit_changed_idle);

  clear_list_of_views (&self->priv->adium_views);
  g_free (self->priv->adium_variant);
  tp_clear_pointer (&self->priv->adium_data, empathy_adium_data_unref);

  G_OBJECT_CLASS (empathy_theme_manager_parent_class)->finalize (object);
}

static void
empathy_theme_manager_class_init (EmpathyThemeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  signals[THEME_CHANGED] = g_signal_new ("theme-changed",
      G_OBJECT_CLASS_TYPE (object_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      0);

  g_type_class_add_private (object_class, sizeof (EmpathyThemeManagerPriv));

  object_class->finalize = theme_manager_finalize;
}

static void
empathy_theme_manager_init (EmpathyThemeManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
    EMPATHY_TYPE_THEME_MANAGER, EmpathyThemeManagerPriv);

  self->priv->in_constructor = TRUE;

  self->priv->gsettings_chat = g_settings_new (EMPATHY_PREFS_CHAT_SCHEMA);

  /* Take the adium path/variant and track changes */
  g_signal_connect (self->priv->gsettings_chat,
      "changed::" EMPATHY_PREFS_CHAT_THEME,
      G_CALLBACK (theme_manager_notify_theme_cb), self);

  theme_manager_notify_theme_cb (self->priv->gsettings_chat,
      EMPATHY_PREFS_CHAT_THEME, self);

  g_signal_connect (self->priv->gsettings_chat,
      "changed::" EMPATHY_PREFS_CHAT_THEME_VARIANT,
      G_CALLBACK (theme_manager_notify_adium_variant_cb), self);

  theme_manager_notify_adium_variant_cb (self->priv->gsettings_chat,
      EMPATHY_PREFS_CHAT_THEME_VARIANT, self);

  self->priv->in_constructor = FALSE;
}

EmpathyThemeManager *
empathy_theme_manager_dup_singleton (void)
{
  static EmpathyThemeManager *manager = NULL;

  if (manager == NULL)
    {
      manager = g_object_new (EMPATHY_TYPE_THEME_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (manager), (gpointer *) &manager);

      return manager;
    }

  return g_object_ref (manager);
}

static void
find_themes (GHashTable *hash,
    const gchar *dirpath)
{
  GDir *dir;
  GError *error = NULL;
  const gchar *name = NULL;
  GHashTable *info = NULL;

  dir = g_dir_open (dirpath, 0, &error);
  if (dir != NULL)
    {
      name = g_dir_read_name (dir);

      while (name != NULL)
        {
          gchar *path;

          path = g_build_path (G_DIR_SEPARATOR_S, dirpath, name, NULL);
          if (empathy_adium_path_is_valid (path))
            {
              info = empathy_adium_info_new (path);

              if (info != NULL)
                {
                  g_hash_table_insert (hash,
                      empathy_theme_manager_dup_theme_name_from_path (path),
                      info);
                }
            }

          g_free (path);
          name = g_dir_read_name (dir);
        }

      g_dir_close (dir);
    }
  else
    {
      DEBUG ("Error opening %s: %s\n", dirpath, error->message);
      g_error_free (error);
    }
}

GList *
empathy_theme_manager_get_adium_themes (void)
{
  /* Theme name -> GHashTable info */
  GHashTable *hash;
  GList *result;
  gchar *path = NULL;
  const gchar *const *paths = NULL;
  gint i = 0;
  const gchar *dir;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Start from the more general locations (the system) to the more specific
   * ones ($HOME, EMPATHY_SRCDIR) so the more specific themes will override
   * the more general ones.*/

  /* System */
  paths = g_get_system_data_dirs ();
  for (i = 0; paths[i] != NULL; i++)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, paths[i],
        "adium/message-styles", NULL);

      find_themes (hash, path);
      g_free (path);
    }

  /* Home */
  path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
      "adium/message-styles", NULL);

  find_themes (hash, path);
  g_free (path);

  /* EMPATHY_SRCDIR */
  dir = g_getenv ("EMPATHY_SRCDIR");
  if (dir != NULL)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, dir, "data/themes/", NULL);

      find_themes (hash, path);
      g_free (path);
    }

  /* Pass ownership of the info hash table to the list */
  result = g_list_copy (g_hash_table_get_values (hash));

  g_hash_table_unref (hash);

  return result;
}

gchar *
empathy_theme_manager_find_theme (const gchar *name)
{
  gchar *path;
  const gchar * const *paths;
  gint i;

  /* look in EMPATHY_SRCDIR */
  path = g_strjoin (NULL,
      g_getenv ("EMPATHY_SRCDIR"),
      "/data/themes/",
      name,
      ".AdiumMessageStyle",
      NULL);

  DEBUG ("Trying '%s'", path);

  if (empathy_adium_path_is_valid (path))
    return path;

  g_free (path);

  /* look in user dir */
  path = g_strjoin (NULL,
      g_get_user_data_dir (),
      "/adium/message-styles/",
      name,
      ".AdiumMessageStyle",
      NULL);

  DEBUG ("Trying '%s'", path);

  if (empathy_adium_path_is_valid (path))
    return path;

  g_free (path);

  /* look in system dirs */
  paths = g_get_system_data_dirs ();

  for (i = 0; paths[i] != NULL; i++)
    {
      path = g_strjoin (NULL,
          paths[i],
          "/adium/message-styles/",
          name,
          ".AdiumMessageStyle",
          NULL);

      DEBUG ("Trying '%s'", path);

      if (empathy_adium_path_is_valid (path))
        return path;

      g_free (path);
    }

  return NULL;
}

gchar *
empathy_theme_manager_dup_theme_name_from_path (const gchar *path)
{
  gchar *fullname, *result;
  gchar **tmp;

  if (path == NULL)
    return NULL;

  fullname = g_path_get_basename (path);
  if (!g_str_has_suffix (fullname, ".AdiumMessageStyle"))
    return NULL;

  tmp = g_strsplit (fullname, ".AdiumMessageStyle", 0);
  result = g_strdup (tmp[0]);

  g_strfreev (tmp);
  return result;
}
