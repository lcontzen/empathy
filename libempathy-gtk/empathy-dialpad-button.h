/*
 * Copyright (C) 2011-2012 Collabora Ltd.
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
 * Authors: Danielle Madeley <danielle.madeley@collabora.co.uk>
 *          Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 */


#ifndef __EMPATHY_DIALPAD_BUTTON_H__
#define __EMPATHY_DIALPAD_BUTTON_H__

#include <gtk/gtk.h>

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef struct _EmpathyDialpadButton EmpathyDialpadButton;
typedef struct _EmpathyDialpadButtonClass EmpathyDialpadButtonClass;
typedef struct _EmpathyDialpadButtonPriv EmpathyDialpadButtonPriv;

struct _EmpathyDialpadButtonClass
{
  /*<private>*/
  GtkButtonClass parent_class;
};

struct _EmpathyDialpadButton
{
  /*<private>*/
  GtkButton parent;
  EmpathyDialpadButtonPriv *priv;
};

GType empathy_dialpad_button_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_DIALPAD_BUTTON \
  (empathy_dialpad_button_get_type ())
#define EMPATHY_DIALPAD_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_DIALPAD_BUTTON, \
    EmpathyDialpadButton))
#define EMPATHY_DIALPAD_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_DIALPAD_BUTTON, \
    EmpathyDialpadButtonClass))
#define EMPATHY_IS_DIALPAD_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_DIALPAD_BUTTON))
#define EMPATHY_IS_DIALPAD_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_DIALPAD_BUTTON))
#define EMPATHY_DIALPAD_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_DIALPAD_BUTTON, \
    EmpathyDialpadButtonClass))

GtkWidget * empathy_dialpad_button_new (
    const gchar *label,
    const gchar *sub_label,
    TpDTMFEvent event);

const gchar * empathy_dialpad_button_get_label (EmpathyDialpadButton *self);
const gchar * empathy_dialpad_button_get_sub_label (EmpathyDialpadButton *self);
TpDTMFEvent empathy_dialpad_button_get_event (EmpathyDialpadButton *self);

G_END_DECLS

#endif /* #ifndef __EMPATHY_DIALPAD_BUTTON_H__*/
