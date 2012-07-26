/*
 * empathy-app-plugin-widget.h
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


#ifndef __EMPATHY_APP_PLUGIN_WIDGET_H__
#define __EMPATHY_APP_PLUGIN_WIDGET_H__

#include <gtk/gtk.h>

#include <libaccounts-glib/ag-account.h>

G_BEGIN_DECLS

typedef struct _EmpathyAppPluginWidget EmpathyAppPluginWidget;
typedef struct _EmpathyAppPluginWidgetClass EmpathyAppPluginWidgetClass;
typedef struct _EmpathyAppPluginWidgetPriv EmpathyAppPluginWidgetPriv;

struct _EmpathyAppPluginWidgetClass
{
  /*<private>*/
  GtkBoxClass parent;
};

struct _EmpathyAppPluginWidget
{
  /*<private>*/
  GtkBox parent_class;
  EmpathyAppPluginWidgetPriv *priv;
};

GType empathy_app_plugin_widget_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_APP_PLUGIN_WIDGET \
  (empathy_app_plugin_widget_get_type ())
#define EMPATHY_APP_PLUGIN_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_APP_PLUGIN_WIDGET, \
    EmpathyAppPluginWidget))
#define EMPATHY_APP_PLUGIN_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_APP_PLUGIN_WIDGET, \
    EmpathyAppPluginWidgetClass))
#define EMPATHY_IS_APP_PLUGIN_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_APP_PLUGIN_WIDGET))
#define EMPATHY_IS_APP_PLUGIN_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_APP_PLUGIN_WIDGET))
#define EMPATHY_APP_PLUGIN_WIDGET_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_APP_PLUGIN_WIDGET, \
    EmpathyAppPluginWidgetClass))

GtkWidget * empathy_app_plugin_widget_new (AgAccount *account);

G_END_DECLS

#endif /* #ifndef __EMPATHY_APP_PLUGIN_WIDGET_H__*/
