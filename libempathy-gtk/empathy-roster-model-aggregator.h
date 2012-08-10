/*
 * empathy-roster-model-aggregator.h
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


#ifndef __EMPATHY_ROSTER_MODEL_AGGREGATOR_H__
#define __EMPATHY_ROSTER_MODEL_AGGREGATOR_H__

#include <glib-object.h>

#include <folks/folks.h>

#include "empathy-roster-model.h"

G_BEGIN_DECLS

typedef struct _EmpathyRosterModelAggregator EmpathyRosterModelAggregator;
typedef struct _EmpathyRosterModelAggregatorClass
EmpathyRosterModelAggregatorClass;
typedef struct _EmpathyRosterModelAggregatorPriv
EmpathyRosterModelAggregatorPriv;

struct _EmpathyRosterModelAggregatorClass
{
  /*<private>*/
  GObjectClass parent_class;
};

struct _EmpathyRosterModelAggregator
{
  /*<private>*/
  GObject parent;
  EmpathyRosterModelAggregatorPriv *priv;
};

typedef gboolean (* EmpathyRosterModelAggregatorFilterFunc) (
    EmpathyRosterModel *model,
    FolksIndividual *individual,
    gpointer user_data);

GType empathy_roster_model_aggregator_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR \
  (empathy_roster_model_aggregator_get_type ())
#define EMPATHY_ROSTER_MODEL_AGGREGATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR, \
    EmpathyRosterModelAggregator))
#define EMPATHY_ROSTER_MODEL_AGGREGATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR, \
    EmpathyRosterModelAggregatorClass))
#define EMPATHY_IS_ROSTER_MODEL_AGGREGATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR))
#define EMPATHY_IS_ROSTER_MODEL_AGGREGATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR))
#define EMPATHY_ROSTER_MODEL_AGGREGATOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ROSTER_MODEL_AGGREGATOR, \
    EmpathyRosterModelAggregatorClass))

EmpathyRosterModelAggregator * empathy_roster_model_aggregator_new (
    EmpathyRosterModelAggregatorFilterFunc filter_func,
    gpointer user_data);

EmpathyRosterModelAggregator *
empathy_roster_model_aggregator_new_with_aggregator (
    FolksIndividualAggregator *aggregator,
    EmpathyRosterModelAggregatorFilterFunc filter_func,
    gpointer user_data);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ROSTER_MODEL_AGGREGATOR_H__*/
