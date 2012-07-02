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

#ifndef __EMPATHY_THEME_MANAGER_H__
#define __EMPATHY_THEME_MANAGER_H__

#include <glib-object.h>
#include "empathy-theme-adium.h"

G_BEGIN_DECLS

/* TYPE MACROS */
#define EMPATHY_TYPE_THEME_MANAGER \
  (empathy_theme_manager_get_type ())
#define EMPATHY_THEME_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_THEME_MANAGER, \
    EmpathyThemeManager))
#define EMPATHY_THEME_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_THEME_MANAGER, \
    EmpathyThemeManagerClass))
#define EMPATHY_IS_THEME_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_THEME_MANAGER))
#define EMPATHY_IS_THEME_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_THEME_MANAGER))
#define EMPATHY_THEME_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_THEME_MANAGER, \
    EmpathyThemeManagerClass))


typedef struct _EmpathyThemeManager      EmpathyThemeManager;
typedef struct _EmpathyThemeManagerClass EmpathyThemeManagerClass;
typedef struct _EmpathyThemeManagerPriv EmpathyThemeManagerPriv;

struct _EmpathyThemeManager
{
  GObject parent;
  EmpathyThemeManagerPriv *priv;
};

struct _EmpathyThemeManagerClass
{
  GObjectClass parent_class;
};

GType empathy_theme_manager_get_type (void) G_GNUC_CONST;
EmpathyThemeManager * empathy_theme_manager_dup_singleton (void);
GList * empathy_theme_manager_get_adium_themes (void);
EmpathyThemeAdium * empathy_theme_manager_create_view (EmpathyThemeManager *self);
gchar * empathy_theme_manager_find_theme (const gchar *name);

gchar * empathy_theme_manager_dup_theme_name_from_path (const gchar *path);

G_END_DECLS

#endif /* __EMPATHY_THEME_MANAGER_H__ */
