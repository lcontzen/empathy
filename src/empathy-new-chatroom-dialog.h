/*
 * Copyright (C) 2006-2007 Imendio AB
 * Copyright (C) 2007-2011 Collabora Ltd.
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_NEW_CHATROOM_DIALOG_H__
#define __EMPATHY_NEW_CHATROOM_DIALOG_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EmpathyNewChatroomDialog EmpathyNewChatroomDialog;
typedef struct _EmpathyNewChatroomDialogClass EmpathyNewChatroomDialogClass;
typedef struct _EmpathyNewChatroomDialogPriv EmpathyNewChatroomDialogPriv;

struct _EmpathyNewChatroomDialogClass
{
  /*<private>*/
  GtkDialogClass parent_class;
};

struct _EmpathyNewChatroomDialog
{
  /*<private>*/
  GtkDialog parent;
  EmpathyNewChatroomDialogPriv *priv;
};

GType empathy_new_chatroom_dialog_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_NEW_CHATROOM_DIALOG \
  (empathy_new_chatroom_dialog_get_type ())
#define EMPATHY_NEW_CHATROOM_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_NEW_CHATROOM_DIALOG, \
    EmpathyNewChatroomDialog))
#define EMPATHY_NEW_CHATROOM_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_NEW_CHATROOM_DIALOG, \
    EmpathyNewChatroomDialogClass))
#define EMPATHY_IS_NEW_CHATROOM_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_NEW_CHATROOM_DIALOG))
#define EMPATHY_IS_NEW_CHATROOM_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_NEW_CHATROOM_DIALOG))
#define EMPATHY_NEW_CHATROOM_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_NEW_CHATROOM_DIALOG, \
    EmpathyNewChatroomDialogClass))

GtkWidget * empathy_new_chatroom_dialog_show (GtkWindow *parent);

G_END_DECLS

#endif /* #ifndef __EMPATHY_NEW_CHATROOM_DIALOG_H__*/
