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
 *
 * Some snippets are taken from GnuTLS 2.8.6, which is distributed under the
 * same GNU Lesser General Public License 2.1 (or later) version. See
 * empathy_get_x509_certified_hostname ().
 */

#include "empathy-roster-utils.h"

gboolean
empathy_folks_persona_is_interesting (FolksPersona *persona)
{
  /* We're not interested in non-Telepathy personas */
  if (!TPF_IS_PERSONA (persona))
    return FALSE;

  /* We're not interested in user personas which haven't been added to the
   * contact list (see bgo#637151). */
  if (folks_persona_get_is_user (persona) &&
      !tpf_persona_get_is_in_contact_list (TPF_PERSONA (persona)))
    {
      return FALSE;
    }

  return TRUE;
}

const gchar * const *
empathy_individual_get_client_types (FolksIndividual *individual)
{
  GeeSet *personas;
  GeeIterator *iter;
  const gchar * const *types = NULL;
  FolksPresenceType presence_type = FOLKS_PRESENCE_TYPE_UNSET;

  personas = folks_individual_get_personas (individual);
  iter = gee_iterable_iterator (GEE_ITERABLE (personas));
  while (gee_iterator_next (iter))
    {
      FolksPresenceDetails *presence;
      FolksPersona *persona = gee_iterator_get (iter);

      /* We only want personas which have presence and a TpContact */
      if (!empathy_folks_persona_is_interesting (persona))
        goto while_finish;

      presence = FOLKS_PRESENCE_DETAILS (persona);

      if (folks_presence_details_typecmp (
              folks_presence_details_get_presence_type (presence),
              presence_type) > 0)
        {
          TpContact *tp_contact;

          presence_type = folks_presence_details_get_presence_type (presence);

          tp_contact = tpf_persona_get_contact (TPF_PERSONA (persona));
          if (tp_contact != NULL)
            types = tp_contact_get_client_types (tp_contact);
        }

    while_finish:
      g_clear_object (&persona);
    }
  g_clear_object (&iter);

  return types;
}
