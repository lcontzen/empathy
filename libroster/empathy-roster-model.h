/*
 * empathy-roster-model.h
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
#ifndef __EMPATHY_ROSTER_MODEL_H__
#define __EMPATHY_ROSTER_MODEL_H__

#include <glib-object.h>

#include <folks/folks.h>

G_BEGIN_DECLS

#define EMPATHY_ROSTER_MODEL_GROUP_TOP_GROUP _("Top Contacts")
#define EMPATHY_ROSTER_MODEL_GROUP_PEOPLE_NEARBY _("People Nearby")
#define EMPATHY_ROSTER_MODEL_GROUP_UNGROUPED _("Ungrouped")

typedef struct _EmpathyRosterModel EmpathyRosterModel;
typedef struct _EmpathyRosterModelInterface EmpathyRosterModelInterface;

struct _EmpathyRosterModelInterface
{
  /*<private>*/
  GTypeInterface g_iface;

  /* Virtual table */
  GList * (* get_individuals) (EmpathyRosterModel *self);
  GList * (*get_groups_for_individual) (EmpathyRosterModel *self,
      FolksIndividual *individual);
};

GType empathy_roster_model_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ROSTER_MODEL \
  (empathy_roster_model_get_type ())
#define EMPATHY_ROSTER_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ROSTER_MODEL, \
    EmpathyRosterModel))
#define EMPATHY_IS_ROSTER_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ROSTER_MODEL))
#define EMPATHY_ROSTER_MODEL_GET_IFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
    EMPATHY_TYPE_ROSTER_MODEL, \
    EmpathyRosterModelInterface))

/* Restricted */

void empathy_roster_model_fire_individual_added (EmpathyRosterModel *self,
    FolksIndividual *individual);

void empathy_roster_model_fire_individual_removed (EmpathyRosterModel *self,
    FolksIndividual *individual);

void empathy_roster_model_fire_groups_changed (EmpathyRosterModel *self,
    FolksIndividual *individual,
    const gchar *group,
    gboolean is_member);

/* Public API */
GList * empathy_roster_model_get_individuals (EmpathyRosterModel *self);

GList * empathy_roster_model_get_groups_for_individual (
    EmpathyRosterModel *self,
    FolksIndividual *individual);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ROSTER_MODEL_H__*/
