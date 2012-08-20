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

/**
 * SECTION: empathy-roster-model-aggregator
 * @title: EmpathyRosterModelAggregator
 * @short_description: RosterModel implementation using a
 * #FolksIndividualAggregator
 *
 * The #FolksIndividualAggregator is used to fetch the contacts and their
 * respective groups.
 *
 * A new #EmpathyRosterModelAggregator object can be created with
 * empathy_roster_model_aggregator_new() or with
 * empathy_roster_model_aggregator_new_with_aggregator().
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
  PROP_FILTER_FUNC,
  PROP_FILTER_DATA,
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
  GHashTable *filtered_individuals; /* Individual -> Individual */

  EmpathyRosterModelAggregatorFilterFunc filter_func;
  gpointer filter_data;
};

static void
individual_group_changed_cb (FolksIndividual *individual,
    gchar *group,
    gboolean is_member,
    EmpathyRosterModelAggregator *self)
{
  empathy_roster_model_fire_groups_changed (EMPATHY_ROSTER_MODEL (self),
      individual, group, is_member);
}

static void
add_to_filtered_individuals (EmpathyRosterModelAggregator *self,
    FolksIndividual *individual)
{
  g_hash_table_add (self->priv->filtered_individuals,
      g_object_ref (individual));

  tp_g_signal_connect_object (individual, "group-changed",
      G_CALLBACK (individual_group_changed_cb), self, 0);

  empathy_roster_model_fire_individual_added (EMPATHY_ROSTER_MODEL (self),
      individual);
}

static void
remove_from_filtered_individuals (EmpathyRosterModelAggregator *self,
    FolksIndividual *individual)
{
  g_signal_handlers_disconnect_by_func (individual,
      individual_group_changed_cb, self);

  g_hash_table_remove (self->priv->filtered_individuals, individual);

  empathy_roster_model_fire_individual_removed (EMPATHY_ROSTER_MODEL (self),
      individual);
}

static void
refilter_individual (FolksIndividual *individual,
    EmpathyRosterModelAggregator *self)
{
  if (!self->priv->filter_func (EMPATHY_ROSTER_MODEL (self), individual, self)
      && g_hash_table_contains (self->priv->filtered_individuals, individual))
    remove_from_filtered_individuals (self, individual);

  if (self->priv->filter_func (EMPATHY_ROSTER_MODEL (self), individual, self)
      && !g_hash_table_contains (self->priv->filtered_individuals, individual))
    add_to_filtered_individuals (self, individual);
}

static void
individual_notify_cb (FolksIndividual *individual,
    GParamSpec *param,
    EmpathyRosterModelAggregator *self)
{
  refilter_individual (individual, self);
}

static void
contact_capabilities_changed_cb (TpContact *contact,
    GParamSpec *pspec,
    EmpathyRosterModelAggregator *self)
{
  TpfPersona *tpf_persona;
  FolksIndividual *individual;

  tpf_persona = tpf_persona_dup_for_contact (contact);
  individual = folks_persona_get_individual (FOLKS_PERSONA (tpf_persona));

  refilter_individual (individual, self);
}

static void
add_individual (EmpathyRosterModelAggregator *self,
    FolksIndividual *individual)
{
  if (self->priv->filter_func != NULL)
    {
      GeeSet *personas;
      GeeIterator *iter;

      tp_g_signal_connect_object (individual, "notify",
          G_CALLBACK (individual_notify_cb), self, 0);

      personas = folks_individual_get_personas (individual);

      iter = gee_iterable_iterator (GEE_ITERABLE (personas));
      while (gee_iterator_next (iter))
        {
          if (TPF_IS_PERSONA (gee_iterator_get (iter)))
            {
              TpContact *tp_contact = tpf_persona_get_contact (
                  gee_iterator_get (iter));

              if (tp_contact != NULL)
                tp_g_signal_connect_object (tp_contact, "notify::capabilities",
                    G_CALLBACK (contact_capabilities_changed_cb), self, 0);
            }
        }

      if (!self->priv->filter_func (EMPATHY_ROSTER_MODEL (self), individual,
              self))
        return;
    }

  add_to_filtered_individuals (self, individual);
}

static void
remove_individual (EmpathyRosterModelAggregator *self,
    FolksIndividual *individual)
{
  if (self->priv->filter_func != NULL)
    {
      GeeSet *personas;
      GeeIterator *iter;

      g_signal_handlers_disconnect_by_func (individual,
          individual_notify_cb, self);

      personas = folks_individual_get_personas (individual);

      iter = gee_iterable_iterator (GEE_ITERABLE (personas));
      while (gee_iterator_next (iter))
        {
          if (TPF_IS_PERSONA (gee_iterator_get (iter)))
            {
              TpContact *tp_contact = tpf_persona_get_contact (
                  gee_iterator_get (iter));

              if (tp_contact != NULL)
                g_signal_handlers_disconnect_by_func (tp_contact,
                    contact_capabilities_changed_cb, self);
            }
        }
    }

  if (g_hash_table_contains (self->priv->filtered_individuals,
          individual))
    remove_from_filtered_individuals (self, individual);
}

static void
populate_individuals (EmpathyRosterModelAggregator *self)
{
  GeeMap *individuals;
  GeeMapIterator *iter;

  individuals = folks_individual_aggregator_get_individuals (
      self->priv->aggregator);
  iter = gee_map_map_iterator (individuals);
  while (gee_map_iterator_next (iter))
    {
      add_individual (self, gee_map_iterator_get_value (iter));
    }
  g_clear_object (&iter);
}

static void
aggregator_individuals_changed_cb (FolksIndividualAggregator *aggregator,
    GeeSet *added,
    GeeSet *removed,
    gchar *message,
    FolksPersona *actor,
    FolksGroupDetailsChangeReason reason,
    EmpathyRosterModelAggregator *self)
{
  if (gee_collection_get_size (GEE_COLLECTION (added)) > 0)
    {
      GeeIterator *iter = gee_iterable_iterator (GEE_ITERABLE (added));

      while (iter != NULL && gee_iterator_next (iter))
        {
          add_individual (self, gee_iterator_get (iter));
        }
      g_clear_object (&iter);
    }

  if (gee_collection_get_size (GEE_COLLECTION (removed)) > 0)
    {
      GeeIterator *iter = gee_iterable_iterator (GEE_ITERABLE (removed));

      while (iter != NULL && gee_iterator_next (iter))
        {
          remove_individual (self, gee_iterator_get (iter));
        }
      g_clear_object (&iter);
    }
}

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
      case PROP_FILTER_FUNC:
        g_value_set_pointer (value, self->priv->filter_func);
        break;
      case PROP_FILTER_DATA:
        g_value_set_pointer (value, self->priv->filter_data);
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
      case PROP_FILTER_FUNC:
        g_assert (self->priv->filter_func == NULL); /* construct only */
        self->priv->filter_func = g_value_get_pointer (value);
        break;
      case PROP_FILTER_DATA:
        g_assert (self->priv->filter_data == NULL); /* construct only */
        self->priv->filter_data = g_value_get_pointer (value);
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

  tp_g_signal_connect_object (self->priv->aggregator, "individuals-changed",
      G_CALLBACK (aggregator_individuals_changed_cb), self, 0);

  folks_individual_aggregator_prepare (self->priv->aggregator, NULL, NULL);

  populate_individuals (self);
}

static void
empathy_roster_model_aggregator_dispose (GObject *object)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_aggregator_parent_class)->dispose;

  g_clear_object (&self->priv->aggregator);
  g_clear_pointer (&self->priv->filtered_individuals, g_hash_table_unref);

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

  spec = g_param_spec_pointer ("filter-func", "Filter-Func",
      "EmpathyRosterModelAggregatorFilterFunc",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_FILTER_FUNC, spec);

  spec = g_param_spec_pointer ("filter-data", "Filter-Data",
      "GPointer",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_FILTER_DATA, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterModelAggregatorPriv));
}

static void
empathy_roster_model_aggregator_init (EmpathyRosterModelAggregator *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR, EmpathyRosterModelAggregatorPriv);

  self->priv->filtered_individuals = g_hash_table_new_full (NULL, NULL, NULL,
      g_object_unref);
}

/**
 * empathy_roster_model_aggregator_new:
 * @filter_func: (scope notified) (allow-none): a #EmpathyRosterModelAggregatorFilterFunc
 * @user_data: (allow-none): optional data to pass to filter-func
 *
 * Creates a new #EmpathyRosterModelAggregator.
 */
EmpathyRosterModelAggregator *
empathy_roster_model_aggregator_new (
    EmpathyRosterModelAggregatorFilterFunc filter_func,
    gpointer user_data)
{
  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR,
      "filter-func", filter_func,
      "filter-data", user_data,
      NULL);
}

/**
 * empathy_roster_model_aggregator_new_with_aggregator:
 * @aggregator: a #FolksIndividualAggregator
 * @filter_func: (scope notified) (allow-none): a #EmpathyRosterModelAggregatorFilterFunc
 * @user_data: (allow-none): optional data to pass to filter-func
 *
 * Creates a new #EmpathyRosterModelAggregator using the given
 * @aggregator.
 */
EmpathyRosterModelAggregator *
empathy_roster_model_aggregator_new_with_aggregator (
    FolksIndividualAggregator *aggregator,
    EmpathyRosterModelAggregatorFilterFunc filter_func,
    gpointer user_data)
{
  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL_AGGREGATOR (aggregator), NULL);

  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR,
      "aggregator", aggregator,
      "filter-func", filter_func,
      "filter-data", user_data,
      NULL);
}

static GList *
empathy_roster_model_aggregator_get_individuals (EmpathyRosterModel *model)
{
  EmpathyRosterModelAggregator *self = EMPATHY_ROSTER_MODEL_AGGREGATOR (model);

  return g_hash_table_get_values (self->priv->filtered_individuals);
}

static GList *
empathy_roster_model_aggregator_get_groups_for_individual (
    EmpathyRosterModel *model,
    FolksIndividual *individual)
{
  GList *groups_list = NULL;
  GeeSet *groups_set;

  groups_set = folks_group_details_get_groups (
      FOLKS_GROUP_DETAILS (individual));
  if (gee_collection_get_size (GEE_COLLECTION (groups_set)) > 0)
    {
      GeeIterator *iter = gee_iterable_iterator (GEE_ITERABLE (groups_set));

      while (iter != NULL && gee_iterator_next (iter))
        {
          groups_list = g_list_prepend (groups_list, gee_iterator_get (iter));
        }
      g_clear_object (&iter);
    }

  return groups_list;
}

static void
roster_model_iface_init (EmpathyRosterModelInterface *iface)
{
  iface->get_individuals = empathy_roster_model_aggregator_get_individuals;
  iface->get_groups_for_individual =
    empathy_roster_model_aggregator_get_groups_for_individual;
}
