/*
 * Copyright (C) 2002-2007 Imendio AB
 * Copyright (C) 2007-2010 Collabora Ltd.
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

#include <telepathy-glib/util.h>
#include <folks/folks.h>

#include <libroster/empathy-roster-utils.h>

#include "empathy-roster-ui-utils.h"

/**
 * stripped_char:
 *
 * Returns a stripped version of @ch, removing any case, accentuation
 * mark, or any special mark on it.
 **/
static gunichar
stripped_char (gunichar ch)
{
  gunichar retval = 0;
  GUnicodeType utype;

  utype = g_unichar_type (ch);

  switch (utype)
    {
    case G_UNICODE_CONTROL:
    case G_UNICODE_FORMAT:
    case G_UNICODE_UNASSIGNED:
    case G_UNICODE_NON_SPACING_MARK:
    case G_UNICODE_COMBINING_MARK:
    case G_UNICODE_ENCLOSING_MARK:
      /* Ignore those */
      break;
    case G_UNICODE_PRIVATE_USE:
    case G_UNICODE_SURROGATE:
    case G_UNICODE_LOWERCASE_LETTER:
    case G_UNICODE_MODIFIER_LETTER:
    case G_UNICODE_OTHER_LETTER:
    case G_UNICODE_TITLECASE_LETTER:
    case G_UNICODE_UPPERCASE_LETTER:
    case G_UNICODE_DECIMAL_NUMBER:
    case G_UNICODE_LETTER_NUMBER:
    case G_UNICODE_OTHER_NUMBER:
    case G_UNICODE_CONNECT_PUNCTUATION:
    case G_UNICODE_DASH_PUNCTUATION:
    case G_UNICODE_CLOSE_PUNCTUATION:
    case G_UNICODE_FINAL_PUNCTUATION:
    case G_UNICODE_INITIAL_PUNCTUATION:
    case G_UNICODE_OTHER_PUNCTUATION:
    case G_UNICODE_OPEN_PUNCTUATION:
    case G_UNICODE_CURRENCY_SYMBOL:
    case G_UNICODE_MODIFIER_SYMBOL:
    case G_UNICODE_MATH_SYMBOL:
    case G_UNICODE_OTHER_SYMBOL:
    case G_UNICODE_LINE_SEPARATOR:
    case G_UNICODE_PARAGRAPH_SEPARATOR:
    case G_UNICODE_SPACE_SEPARATOR:
    default:
      ch = g_unichar_tolower (ch);
      g_unichar_fully_decompose (ch, FALSE, &retval, 1);
    }

  return retval;
}

static gboolean
live_search_match_prefix (const gchar *string,
    const gchar *prefix)
{
  const gchar *p;
  const gchar *prefix_p;
  gboolean next_word = FALSE;

  if (prefix == NULL || prefix[0] == 0)
    return TRUE;

  if (EMP_STR_EMPTY (string))
    return FALSE;

  prefix_p = prefix;
  for (p = string; *p != '\0'; p = g_utf8_next_char (p))
    {
      gunichar sc;

      /* Make the char lower-case, remove its accentuation marks, and ignore it
       * if it is just unicode marks */
      sc = stripped_char (g_utf8_get_char (p));
      if (sc == 0)
        continue;

      /* If we want to go to next word, ignore alpha-num chars */
      if (next_word && g_unichar_isalnum (sc))
        continue;
      next_word = FALSE;

      /* Ignore word separators */
      if (!g_unichar_isalnum (sc))
        continue;

      /* If this char does not match prefix_p, go to next word and start again
       * from the beginning of prefix */
      if (sc != g_utf8_get_char (prefix_p))
        {
          next_word = TRUE;
          prefix_p = prefix;
          continue;
        }

      /* prefix_p match, verify to next char. If this was the last of prefix,
       * it means it completely machted and we are done. */
      prefix_p = g_utf8_next_char (prefix_p);
      if (*prefix_p == '\0')
        return TRUE;
    }

  return FALSE;
}

static gboolean
live_search_match_words (const gchar *string,
    GPtrArray *words)
{
  guint i;

  if (words == NULL)
    return TRUE;

  for (i = 0; i < words->len; i++)
    if (!live_search_match_prefix (string, g_ptr_array_index (words, i)))
      return FALSE;

  return TRUE;
}

/* @words = empathy_live_search_strip_utf8_string (@text);
 *
 * User has to pass both so we don't have to compute @words ourself each time
 * this function is called. */
gboolean
empathy_individual_match_string (FolksIndividual *individual,
    const char *text,
    GPtrArray *words)
{
  const gchar *str;
  GeeSet *personas;
  GeeIterator *iter;
  gboolean retval = FALSE;

  /* check alias name */
  str = folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (individual));

  if (live_search_match_words (str, words))
    return TRUE;

  personas = folks_individual_get_personas (individual);

  /* check contact id, remove the @server.com part */
  iter = gee_iterable_iterator (GEE_ITERABLE (personas));
  while (retval == FALSE && gee_iterator_next (iter))
    {
      FolksPersona *persona = gee_iterator_get (iter);
      const gchar *p;

      if (empathy_folks_persona_is_interesting (persona))
        {
          str = folks_persona_get_display_id (persona);

          /* Accept the persona if @text is a full prefix of his ID; that allows
           * user to find, say, a jabber contact by typing his JID. */
          if (g_str_has_prefix (str, text))
            {
              retval = TRUE;
            }
          else
            {
              gchar *dup_str = NULL;
              gboolean visible;

              p = strstr (str, "@");
              if (p != NULL)
                str = dup_str = g_strndup (str, p - str);

              visible = live_search_match_words (str, words);
              g_free (dup_str);
              if (visible)
                retval = TRUE;
            }
        }
      g_clear_object (&persona);
    }
  g_clear_object (&iter);

  /* FIXME: Add more rules here, we could check phone numbers in
   * contact's vCard for example. */
  return retval;
}
