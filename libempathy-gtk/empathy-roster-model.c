/*
 * empathy-roster-model.c
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

#include "empathy-roster-model.h"

G_DEFINE_INTERFACE (EmpathyRosterModel, empathy_roster_model, G_TYPE_OBJECT)

enum
{
  SIG_INDIVIDUAL_ADDED,
  SIG_INDIVIDUAL_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
empathy_roster_model_default_init (EmpathyRosterModelInterface *iface)
{
  signals[SIG_INDIVIDUAL_ADDED] =
    g_signal_new ("individual-added",
        EMPATHY_TYPE_ROSTER_MODEL,
        G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL,
        G_TYPE_NONE, 1,
        FOLKS_TYPE_INDIVIDUAL);

  signals[SIG_INDIVIDUAL_REMOVED] =
    g_signal_new ("individual-removed",
        EMPATHY_TYPE_ROSTER_MODEL,
        G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL,
        G_TYPE_NONE, 1,
        FOLKS_TYPE_INDIVIDUAL);
}

/***** Restricted *****/

void
empathy_roster_model_fire_individual_added (EmpathyRosterModel *self,
    FolksIndividual *individual)
{
  g_signal_emit (self, signals[SIG_INDIVIDUAL_ADDED], 0, individual);
}

void
empathy_roster_model_fire_individual_removed (EmpathyRosterModel *self,
    FolksIndividual *individual)
{
  g_signal_emit (self, signals[SIG_INDIVIDUAL_REMOVED], 0, individual);
}

/***** Public *****/

GList *
empathy_roster_model_get_individuals (EmpathyRosterModel *self)
{
  EmpathyRosterModelInterface *iface;

  g_return_val_if_fail (EMPATHY_IS_ROSTER_MODEL (self), NULL);

  iface = EMPATHY_ROSTER_MODEL_GET_IFACE (self);
  g_return_val_if_fail (iface->get_individuals != NULL, NULL);

  return (* iface->get_individuals) (self);
}
