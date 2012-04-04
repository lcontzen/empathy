/*
 * empathy-local-xmpp-assistant-widget.h - Source for
 * EmpathyLocalXmppAssistantWidget
 *
 * Copyright (C) 2012 - Collabora Ltd.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with This library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET_H__
#define __EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET_H__

#include <gtk/gtk.h>

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET (empathy_local_xmpp_assistant_widget_get_type ())
#define EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET, EmpathyLocalXmppAssistantWidget))
#define EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET, EmpathyLocalXmppAssistantWidgetClass))
#define EMPATHY_IS_LOCAL_XMPP_ASSISTANT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET))
#define EMPATHY_IS_LOCAL_XMPP_ASSISTANT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET))
#define EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_LOCAL_XMPP_ASSISTANT_WIDGET, EmpathyLocalXmppAssistantWidgetClass))

typedef struct _EmpathyLocalXmppAssistantWidget EmpathyLocalXmppAssistantWidget;
typedef struct _EmpathyLocalXmppAssistantWidgetClass EmpathyLocalXmppAssistantWidgetClass;
typedef struct _EmpathyLocalXmppAssistantWidgetPrivate EmpathyLocalXmppAssistantWidgetPrivate;

struct _EmpathyLocalXmppAssistantWidget {
  GtkGrid parent;

  EmpathyLocalXmppAssistantWidgetPrivate *priv;
};

struct _EmpathyLocalXmppAssistantWidgetClass {
  GtkGridClass parent_class;
};

GType empathy_local_xmpp_assistant_widget_get_type (void) G_GNUC_CONST;

GtkWidget * empathy_local_xmpp_assistant_widget_new (void);

void empathy_local_xmpp_assistant_widget_create_account (
    EmpathyLocalXmppAssistantWidget *self);

gboolean empathy_local_xmpp_assistant_widget_should_create_account (
    TpAccountManager *manager);

gboolean empathy_local_xmpp_assistant_widget_is_valid (
        EmpathyLocalXmppAssistantWidget *self);

G_END_DECLS

#endif /* __EMPATHY_LOCAL_XMPP_ASSISTANT_WIDGET_H__ */
