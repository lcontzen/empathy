/*
 * empathy-roster-model-manager.c
 *
 * Implementation of EmpathyRosterModel using EmpathyIndividualManager as
 * source.
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

#include "config.h"

#include "empathy-roster-model-manager.h"

#include "empathy-roster-model.h"

static void roster_model_iface_init (EmpathyRosterModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EmpathyRosterModelManager,
    empathy_roster_model_manager,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_ROSTER_MODEL, roster_model_iface_init))

enum
{
  PROP_FIRST_PROP = 1,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyRosterModelManagerPriv
{
  gpointer badger;
};

static void
empathy_roster_model_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  //EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);

  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_model_manager_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  //EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);

  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_model_manager_constructed (GObject *object)
{
  //EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_manager_dispose (GObject *object)
{
  //EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->dispose;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_manager_finalize (GObject *object)
{
  //EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->finalize;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_manager_class_init (
    EmpathyRosterModelManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  //GParamSpec *spec;

  oclass->get_property = empathy_roster_model_manager_get_property;
  oclass->set_property = empathy_roster_model_manager_set_property;
  oclass->constructed = empathy_roster_model_manager_constructed;
  oclass->dispose = empathy_roster_model_manager_dispose;
  oclass->finalize = empathy_roster_model_manager_finalize;

  g_type_class_add_private (klass, sizeof (EmpathyRosterModelManagerPriv));
}

static void
empathy_roster_model_manager_init (EmpathyRosterModelManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_MODEL_MANAGER, EmpathyRosterModelManagerPriv);
}

EmpathyRosterModelManager *
empathy_roster_model_manager_new (void)
{
  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_MANAGER,
      NULL);
}

static void
roster_model_iface_init (EmpathyRosterModelInterface *iface)
{
}
