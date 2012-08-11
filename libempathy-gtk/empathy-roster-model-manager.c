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

#include <libroster/empathy-roster-model.h>

#include <glib/gi18n-lib.h>

#include <libempathy/empathy-utils.h>

static void roster_model_iface_init (EmpathyRosterModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EmpathyRosterModelManager,
    empathy_roster_model_manager,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_ROSTER_MODEL, roster_model_iface_init))

enum
{
  PROP_MANAGER = 1,
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
  EmpathyIndividualManager *manager;
  /* FolksIndividual (borrowed) */
  GList *top_group_members;
};

static gboolean
is_xmpp_local_contact (FolksIndividual *individual)
{
  EmpathyContact *contact;
  TpConnection *connection;
  const gchar *protocol_name = NULL;
  gboolean result;

  contact = empathy_contact_dup_from_folks_individual (individual);

  if (contact == NULL)
    return FALSE;

  connection = empathy_contact_get_connection (contact);
  protocol_name = tp_connection_get_protocol_name (connection);
  result = !tp_strdiff (protocol_name, "local-xmpp");
  g_object_unref (contact);

  return result;
}

static gboolean
individual_in_top_group_members (EmpathyRosterModelManager *self,
    FolksIndividual *individual)
{
  return (g_list_find (self->priv->top_group_members, individual) != NULL);
}

static gboolean
individual_should_be_in_top_group_members (EmpathyRosterModelManager *self,
    FolksIndividual *individual)
{
  GList *tops;

  tops = empathy_individual_manager_get_top_individuals (self->priv->manager);

  return (folks_favourite_details_get_is_favourite (
          FOLKS_FAVOURITE_DETAILS (individual)) ||
      g_list_find (tops, individual) != NULL);
}

static void
populate_model (EmpathyRosterModelManager *self)
{
  GList *individuals, *l;

  individuals = empathy_individual_manager_get_members (self->priv->manager);

  for (l = individuals; l != NULL; l = g_list_next (l))
    {
      if (individual_should_be_in_top_group_members (self, l->data))
        self->priv->top_group_members = g_list_prepend (
            self->priv->top_group_members, l->data);

      empathy_roster_model_fire_individual_added (EMPATHY_ROSTER_MODEL (self),
          l->data);
    }
}

static void
members_changed_cb (EmpathyIndividualManager *manager,
    const gchar *message,
    GList *added,
    GList *removed,
    TpChannelGroupChangeReason reason,
    EmpathyRosterModelManager *self)
{
  GList *l;

  for (l = added; l != NULL; l = g_list_next (l))
    {
      if (individual_should_be_in_top_group_members (self, l->data) &&
          !individual_in_top_group_members (self, l->data))
        self->priv->top_group_members = g_list_prepend (
            self->priv->top_group_members, l->data);

      empathy_roster_model_fire_individual_added (EMPATHY_ROSTER_MODEL (self),
          l->data);
    }

  for (l = removed; l != NULL; l = g_list_next (l))
    {
      if (individual_in_top_group_members (self, l->data) &&
          !individual_should_be_in_top_group_members (self, l->data))
        self->priv->top_group_members = g_list_remove (
            self->priv->top_group_members, l->data);

      empathy_roster_model_fire_individual_removed (EMPATHY_ROSTER_MODEL (self),
          l->data);
    }
}

static void
groups_changed_cb (EmpathyIndividualManager *manager,
    FolksIndividual *individual,
    const gchar *group,
    gboolean is_member,
    EmpathyRosterModelManager *self)
{
  empathy_roster_model_fire_groups_changed (EMPATHY_ROSTER_MODEL (self),
      individual, group, is_member);
}

static void
top_individuals_changed_cb (EmpathyIndividualManager *manager,
    GParamSpec *spec,
    EmpathyRosterModelManager *self)
{
  GList *tops, *l;

  tops = empathy_individual_manager_get_top_individuals (self->priv->manager);

  for (l = tops; l != NULL; l = g_list_next (l))
    {
      if (!individual_in_top_group_members (self, l->data))
        {
          self->priv->top_group_members = g_list_prepend (
              self->priv->top_group_members, l->data);

          empathy_roster_model_fire_groups_changed (
              EMPATHY_ROSTER_MODEL (self), l->data,
              EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP, TRUE);
        }
    }
  for (l = self->priv->top_group_members; l != NULL; l = g_list_next (l))
    {
      if (!individual_should_be_in_top_group_members (self, l->data))
        {
          self->priv->top_group_members = g_list_remove (
              self->priv->top_group_members, l->data);

          empathy_roster_model_fire_groups_changed (
              EMPATHY_ROSTER_MODEL (self), l->data,
              EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP, FALSE);
        }
    }
}

static void
favourites_changed_cb (EmpathyIndividualManager *manager,
    FolksIndividual *individual,
    gboolean favourite,
    EmpathyRosterModelManager *self)
{
  if (favourite && !individual_in_top_group_members (self, individual))
    {
      self->priv->top_group_members = g_list_prepend (
          self->priv->top_group_members, individual);

      empathy_roster_model_fire_groups_changed (
          EMPATHY_ROSTER_MODEL (self), individual,
          EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP, favourite);
    }
  else if (!favourite &&
      !individual_should_be_in_top_group_members (self, individual))
    {
      self->priv->top_group_members = g_list_remove (
          self->priv->top_group_members, individual);

      empathy_roster_model_fire_groups_changed (
          EMPATHY_ROSTER_MODEL (self), individual,
          EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP, favourite);
    }
}

static void
empathy_roster_model_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);

  switch (property_id)
    {
      case PROP_MANAGER:
        g_value_set_object (value, self->priv->manager);
        break;
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
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);

  switch (property_id)
    {
      case PROP_MANAGER:
        g_assert (self->priv->manager == NULL); /* construct only */
        self->priv->manager = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_model_manager_constructed (GObject *object)
{
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (EMPATHY_IS_INDIVIDUAL_MANAGER (self->priv->manager));

  populate_model (self);

  tp_g_signal_connect_object (self->priv->manager, "members-changed",
      G_CALLBACK (members_changed_cb), self, 0);
  tp_g_signal_connect_object (self->priv->manager, "groups-changed",
      G_CALLBACK (groups_changed_cb), self, 0);
  tp_g_signal_connect_object (self->priv->manager, "notify::top-individuals",
      G_CALLBACK (top_individuals_changed_cb), self, 0);
  tp_g_signal_connect_object (self->priv->manager, "notify::favourites-changed",
      G_CALLBACK (favourites_changed_cb), self, 0);
}

static void
empathy_roster_model_manager_dispose (GObject *object)
{
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->dispose;

  g_clear_object (&self->priv->manager);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_manager_finalize (GObject *object)
{
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_model_manager_parent_class)->finalize;

  g_list_free (self->priv->top_group_members);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_model_manager_class_init (
    EmpathyRosterModelManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_roster_model_manager_get_property;
  oclass->set_property = empathy_roster_model_manager_set_property;
  oclass->constructed = empathy_roster_model_manager_constructed;
  oclass->dispose = empathy_roster_model_manager_dispose;
  oclass->finalize = empathy_roster_model_manager_finalize;

  spec = g_param_spec_object ("manager", "Manager",
      "EmpathyIndividualManager",
      EMPATHY_TYPE_INDIVIDUAL_MANAGER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_MANAGER, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterModelManagerPriv));
}

static void
empathy_roster_model_manager_init (EmpathyRosterModelManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_MODEL_MANAGER, EmpathyRosterModelManagerPriv);

  self->priv->top_group_members = NULL;
}

EmpathyRosterModelManager *
empathy_roster_model_manager_new (EmpathyIndividualManager *manager)
{
  g_return_val_if_fail (EMPATHY_IS_INDIVIDUAL_MANAGER (manager), NULL);

  return g_object_new (EMPATHY_TYPE_ROSTER_MODEL_MANAGER,
      "manager", manager,
      NULL);
}

static GList *
empathy_roster_model_manager_get_individuals (EmpathyRosterModel *model)
{
  EmpathyRosterModelManager *self = EMPATHY_ROSTER_MODEL_MANAGER (model);

  return empathy_individual_manager_get_members (self->priv->manager);
}

static GList *
empathy_roster_model_manager_get_groups_for_individual (
    EmpathyRosterModel *model,
    FolksIndividual *individual)
{
  GList *groups_list = NULL;
  GeeSet *groups_set;

  if (is_xmpp_local_contact (individual))
    {
      groups_list = g_list_prepend (groups_list,
          EMPATHY_ROSTER_MODEL_GROUP_PEOPLE_NEARBY);
      return groups_list;
    }

  if (individual_in_top_group_members (EMPATHY_ROSTER_MODEL_MANAGER (model),
          individual))
    groups_list = g_list_prepend (groups_list,
        EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP);

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
  iface->get_individuals = empathy_roster_model_manager_get_individuals;
  iface->get_groups_for_individual =
    empathy_roster_model_manager_get_groups_for_individual;
}
