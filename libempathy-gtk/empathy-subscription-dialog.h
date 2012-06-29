/*
 * Copyright (C) 2012 Collabora Ltd.
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
 * Authors: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 */

#ifndef __EMPATHY_SUBSCRIPTION_DIALOG_H__
#define __EMPATHY_SUBSCRIPTION_DIALOG_H__

#include <gtk/gtk.h>
#include <folks/folks.h>

G_BEGIN_DECLS

typedef struct _EmpathySubscriptionDialog EmpathySubscriptionDialog;
typedef struct _EmpathySubscriptionDialogClass EmpathySubscriptionDialogClass;
typedef struct _EmpathySubscriptionDialogPriv EmpathySubscriptionDialogPriv;

struct _EmpathySubscriptionDialogClass
{
  /*<private>*/
  GtkMessageDialogClass parent_class;
};

struct _EmpathySubscriptionDialog
{
  /*<private>*/
  GtkMessageDialog parent;
  EmpathySubscriptionDialogPriv *priv;
};

GType empathy_subscription_dialog_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_SUBSCRIPTION_DIALOG \
  (empathy_subscription_dialog_get_type ())
#define EMPATHY_SUBSCRIPTION_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_SUBSCRIPTION_DIALOG, \
    EmpathySubscriptionDialog))
#define EMPATHY_SUBSCRIPTION_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_SUBSCRIPTION_DIALOG, \
    EmpathySubscriptionDialogClass))
#define EMPATHY_IS_SUBSCRIPTION_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_SUBSCRIPTION_DIALOG))
#define EMPATHY_IS_SUBSCRIPTION_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_SUBSCRIPTION_DIALOG))
#define EMPATHY_SUBSCRIPTION_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_SUBSCRIPTION_DIALOG, \
    EmpathySubscriptionDialogClass))

GtkWidget * empathy_subscription_dialog_new (FolksIndividual *individual,
    const gchar *message);

G_END_DECLS

#endif /* #ifndef __EMPATHY_SUBSCRIPTION_DIALOG_H__*/
