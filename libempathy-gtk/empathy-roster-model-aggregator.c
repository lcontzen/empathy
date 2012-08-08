/*
 * empathy-roster-model-aggregator.c
 *
 * Implementation of EmpathyRosterModel using FolksIndividualAggregator as
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

#include <folks/folks.h>
#include <folks/folks-telepathy.h>

#include "empathy-roster-model-aggregator.h"

#include "empathy-roster-model.h"

/**
 * SECTION: empathy-roster-model-aggregator
 * @title: EmpathyRosterModelAggregator
 * @short_description: TODO
 *
 * TODO
 */

/**
 * EmpathyRosterModelAggregator:
 *
 * Data structure representing a #EmpathyRosterModelAggregator.
 *
 * Since: UNRELEASED
 */

/**
 * EmpathyRosterModelAggregatorClass:
 *
 * The class of a #EmpathyRosterModelAggregator.
 *
 * Since: UNRELEASED
 */

static void roster_model_iface_init (EmpathyRosterModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EmpathyRosterModelAggregator,
    empathy_roster_model_aggregator,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_ROSTER_MODEL, roster_model_iface_init))

enum
{
  PROP_AGGREGATOR = 1,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyRosterModelAggregatorPriv
{
  FolksIndividualAggregator *aggregator;
};

static void
empathy_roster_model_aggregator_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);

  switch (property_id)
    {
      case PROP_AGGREGATOR:
        g_value_set_object (value, self->priv->aggregator);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_model_aggregator_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);

  switch (property_id)
    {
      case PROP_AGGREGATOR:
        g_assert (self->priv->aggregator == NULL); /* construct only */
        self->priv->aggregator = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_model_aggregator_constructed (GObject *object)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_aggregator_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  if (self->priv->aggregator == NULL)
    self->priv->aggregator = folks_individual_aggregator_new ();

  g_assert (FOLKS_IS_INDIVIDUAL_AGGREGATOR (self->priv->aggregator));
}

static void
empathy_roster_model_aggregator_dispose (GObject *object)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_aggregator_parent_class)->dispose;

  g_clear_object (&self->priv->aggregator);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_aggregator_finalize (GObject *object)
{
  //EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_aggregator_parent_class)->finalize;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_aggregator_class_init (
    EmpathyRosterModelAggregatorClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_roster_model_aggregator_get_property;
  oclass->set_property = empathy_roster_model_aggregator_set_property;
  oclass->constructed = empathy_roster_model_aggregator_constructed;
  oclass->dispose = empathy_roster_model_aggregator_dispose;
  oclass->finalize = empathy_roster_model_aggregator_finalize;

  spec = g_param_spec_object ("aggregator", "Aggregator",
      "FolksIndividualAggregator",
      FOLKS_TYPE_INDIVIDUAL_AGGREGATOR,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_AGGREGATOR, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterModelAggregatorPriv));
}

static void
empathy_roster_model_aggregator_init (EmpathyRosterModelAggregator *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR, EmpathyRosterModelAggregatorPriv);
}

EmpathyRosterModelAggregator *
empathy_roster_model_aggregator_new (void)
{
  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR,
      NULL);
}

EmpathyRosterModelAggregator *
empathy_roster_model_aggregator_new_with_aggregator (
    FolksIndividualAggregator *aggregator)
{
  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL_AGGREGATOR (aggregator), NULL);

  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR,
      "aggregator", aggregator,
      NULL);
}

static void
roster_model_iface_init (EmpathyRosterModelInterface *iface)
{

}
