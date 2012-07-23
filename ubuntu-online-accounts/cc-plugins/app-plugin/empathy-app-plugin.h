/*
 * empathy-app-plugin.h
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

#ifndef __EMPATHY_APP_PLUGIN_H__
#define __EMPATHY_APP_PLUGIN_H__

#include <glib-object.h>

#include <libaccount-plugin/application-plugin.h>

G_BEGIN_DECLS

typedef struct _EmpathyAppPlugin EmpathyAppPlugin;
typedef struct _EmpathyAppPluginClass EmpathyAppPluginClass;

struct _EmpathyAppPluginClass
{
  /*<private>*/
 ApApplicationPluginClass parent_class;
};

struct _EmpathyAppPlugin
{
  /*<private>*/
  ApApplicationPlugin parent;
};

GType empathy_app_plugin_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_APP_PLUGIN \
  (empathy_app_plugin_get_type ())
#define EMPATHY_APP_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_APP_PLUGIN, \
    EmpathyAppPlugin))
#define EMPATHY_APP_PLUGIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_APP_PLUGIN, \
    EmpathyAppPluginClass))
#define EMPATHY_IS_APP_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_APP_PLUGIN))
#define EMPATHY_IS_APP_PLUGIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_APP_PLUGIN))
#define EMPATHY_APP_PLUGIN_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_APP_PLUGIN, \
    EmpathyAppPluginClass))

GType ap_module_get_object_type (void);

G_END_DECLS

#endif /* #ifndef __EMPATHY_APP_PLUGIN_H__*/
