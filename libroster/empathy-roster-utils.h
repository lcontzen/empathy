/*
 * Copyright (C) 2003-2007 Imendio AB
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
 */

#ifndef _EMPATHY_ROSTER_UTILS_H_
#define _EMPATHY_ROSTER_UTILS_H_

#include <folks/folks.h>
#include <folks/folks-telepathy.h>

#define EMPATHY_GET_PRIV(obj,type) ((type##Priv *) ((type *) obj)->priv)
#define EMP_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

gboolean empathy_folks_persona_is_interesting (FolksPersona *persona);

const gchar * const * empathy_individual_get_client_types (
    FolksIndividual *individual);

TpConnectionPresenceType empathy_folks_presence_type_to_tp (
    FolksPresenceType type);

#endif /* _EMPATHY_ROSTER_UTILS_H_ */
