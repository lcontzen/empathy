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


#include "config.h"

#include "empathy-dialpad-button.h"

G_DEFINE_TYPE (EmpathyDialpadButton, empathy_dialpad_button, GTK_TYPE_BUTTON)

enum
{
  PROP_LABEL = 1,
  PROP_SUB_LABEL,
  PROP_EVENT,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyDialpadButtonPriv
{
  gchar *label;
  gchar *sub_label;
  TpDTMFEvent event;
};

static void
empathy_dialpad_button_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyDialpadButton *self = EMPATHY_DIALPAD_BUTTON (object);

  switch (property_id)
    {
      case PROP_LABEL:
        g_value_set_string (value, self->priv->label);
        break;
      case PROP_SUB_LABEL:
        g_value_set_string (value, self->priv->sub_label);
        break;
      case PROP_EVENT:
        g_value_set_uint (value, self->priv->event);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_dialpad_button_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyDialpadButton *self = EMPATHY_DIALPAD_BUTTON (object);

  switch (property_id)
    {
      case PROP_LABEL:
        g_assert (self->priv->label == NULL); /* construct-only */
        self->priv->label = g_value_dup_string (value);
        break;
      case PROP_SUB_LABEL:
        g_assert (self->priv->sub_label == NULL); /* construct-only */
        self->priv->sub_label = g_value_dup_string (value);
        break;
      case PROP_EVENT:
        self->priv->event = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_dialpad_button_constructed (GObject *object)
{
  EmpathyDialpadButton *self = EMPATHY_DIALPAD_BUTTON (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_dialpad_button_parent_class)->constructed;
  GtkWidget *vbox;
  GtkWidget *label;
  gchar *str;

  g_assert (self->priv->label != NULL);
  g_assert (self->priv->sub_label != NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (self), vbox);

  /* main label */
  label = gtk_label_new ("");
  str = g_strdup_printf ("<span size='x-large'>%s</span>",
      self->priv->label);
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);

  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 3);

  /* sub label */
  label = gtk_label_new ("");
  str = g_strdup_printf (
      "<span foreground='#555555'>%s</span>",
      self->priv->sub_label);
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_dialpad_button_finalize (GObject *object)
{
  EmpathyDialpadButton *self = EMPATHY_DIALPAD_BUTTON (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_dialpad_button_parent_class)->finalize;

  g_free (self->priv->label);
  g_free (self->priv->sub_label);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_dialpad_button_class_init (
    EmpathyDialpadButtonClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_dialpad_button_get_property;
  oclass->set_property = empathy_dialpad_button_set_property;
  oclass->constructed = empathy_dialpad_button_constructed;
  oclass->finalize = empathy_dialpad_button_finalize;

  spec = g_param_spec_string ("label", "label",
      "Label",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_LABEL, spec);

  spec = g_param_spec_string ("sub-label", "sub-label",
      "Sub-label",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_SUB_LABEL, spec);

  spec = g_param_spec_uint ("event", "event",
      "TpDTMFEvent",
      0, TP_NUM_DTMF_EVENTS, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_EVENT, spec);

  g_type_class_add_private (klass, sizeof (EmpathyDialpadButtonPriv));
}

static void
empathy_dialpad_button_init (EmpathyDialpadButton *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_DIALPAD_BUTTON, EmpathyDialpadButtonPriv);
}

GtkWidget *
empathy_dialpad_button_new (const gchar *label,
    const gchar *sub_label,
    TpDTMFEvent event)
{
  return g_object_new (EMPATHY_TYPE_DIALPAD_BUTTON,
      "label", label,
      "sub-label", sub_label,
      "event", event,
      NULL);
}

const gchar *
empathy_dialpad_button_get_label (EmpathyDialpadButton *self)
{
  return self->priv->label;
}

const gchar *
empathy_dialpad_button_get_sub_label (EmpathyDialpadButton *self)
{
  return self->priv->sub_label;
}

TpDTMFEvent
empathy_dialpad_button_get_event (EmpathyDialpadButton *self)
{
  return self->priv->event;
}
