/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_CONTACT_WIDGET_H__
#define __EMPATHY_CONTACT_WIDGET_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include "empathy-account-chooser.h"

G_BEGIN_DECLS

typedef struct _EmpathyContactWidget EmpathyContactWidget;
typedef struct _EmpathyContactWidgetClass EmpathyContactWidgetClass;
typedef struct _EmpathyContactWidgetPriv EmpathyContactWidgetPriv;

struct _EmpathyContactWidgetClass
{
  /*<private>*/
  GtkBoxClass parent_class;
};

struct _EmpathyContactWidget
{
  /*<private>*/
  GtkBox parent;
  EmpathyContactWidgetPriv *priv;
};

GType empathy_contact_widget_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_CONTACT_WIDGET \
  (empathy_contact_widget_get_type ())
#define EMPATHY_CONTACT_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_CONTACT_WIDGET, \
    EmpathyContactWidget))
#define EMPATHY_CONTACT_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_CONTACT_WIDGET, \
    EmpathyContactWidgetClass))
#define EMPATHY_IS_CONTACT_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_CONTACT_WIDGET))
#define EMPATHY_IS_CONTACT_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_CONTACT_WIDGET))
#define EMPATHY_CONTACT_WIDGET_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_CONTACT_WIDGET, \
    EmpathyContactWidgetClass))

GtkWidget * empathy_contact_widget_new (EmpathyContact *contact);
EmpathyContact *empathy_contact_widget_get_contact (GtkWidget *widget);
void empathy_contact_widget_set_contact (GtkWidget *widget,
    EmpathyContact *contact);
void empathy_contact_widget_set_account_filter (GtkWidget *widget,
    EmpathyAccountChooserFilterFunc filter, gpointer user_data);
const gchar *empathy_contact_widget_get_alias (GtkWidget *widget);

G_END_DECLS

#endif /*  __EMPATHY_CONTACT_WIDGET_H__ */
