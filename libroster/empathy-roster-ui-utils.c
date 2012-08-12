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
#include <libroster/empathy-roster-images.h>

#include "empathy-roster-ui-utils.h"

#define DEBUG g_debug

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

static void
empathy_avatar_pixbuf_roundify (GdkPixbuf *pixbuf)
{
  gint width, height, rowstride;
  guchar *pixels;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  if (width < 6 || height < 6)
    return;

  /* Top left */
  pixels[3] = 0;
  pixels[7] = 0x80;
  pixels[11] = 0xC0;
  pixels[rowstride + 3] = 0x80;
  pixels[rowstride * 2 + 3] = 0xC0;

  /* Top right */
  pixels[width * 4 - 1] = 0;
  pixels[width * 4 - 5] = 0x80;
  pixels[width * 4 - 9] = 0xC0;
  pixels[rowstride + (width * 4) - 1] = 0x80;
  pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

  /* Bottom left */
  pixels[(height - 1) * rowstride + 3] = 0;
  pixels[(height - 1) * rowstride + 7] = 0x80;
  pixels[(height - 1) * rowstride + 11] = 0xC0;
  pixels[(height - 2) * rowstride + 3] = 0x80;
  pixels[(height - 3) * rowstride + 3] = 0xC0;

  /* Bottom right */
  pixels[height * rowstride - 1] = 0;
  pixels[(height - 1) * rowstride - 1] = 0x80;
  pixels[(height - 2) * rowstride - 1] = 0xC0;
  pixels[height * rowstride - 5] = 0x80;
  pixels[height * rowstride - 9] = 0xC0;
}

static gboolean
empathy_gdk_pixbuf_is_opaque (GdkPixbuf *pixbuf)
{
  gint height, rowstride, i;
  guchar *pixels;
  guchar *row;

  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  row = pixels;
  for (i = 3; i < rowstride; i+=4)
    if (row[i] < 0xfe)
      return FALSE;

  for (i = 1; i < height - 1; i++)
    {
      row = pixels + (i*rowstride);
      if (row[3] < 0xfe || row[rowstride-1] < 0xfe)
        return FALSE;
    }

  row = pixels + ((height-1) * rowstride);
  for (i = 3; i < rowstride; i+=4)
    if (row[i] < 0xfe)
      return FALSE;

  return TRUE;
}

static GdkPixbuf *
pixbuf_round_corners (GdkPixbuf *pixbuf)
{
  GdkPixbuf *result;

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
          gdk_pixbuf_get_width (pixbuf),
          gdk_pixbuf_get_height (pixbuf));

      gdk_pixbuf_copy_area (pixbuf, 0, 0,
          gdk_pixbuf_get_width (pixbuf),
          gdk_pixbuf_get_height (pixbuf),
          result,
          0, 0);
    }
  else
    {
      result = g_object_ref (pixbuf);
    }

  if (empathy_gdk_pixbuf_is_opaque (result))
    empathy_avatar_pixbuf_roundify (result);

  return result;
}

typedef struct
{
  GSimpleAsyncResult *result;
  guint width;
  guint height;
  GCancellable *cancellable;
} PixbufAvatarFromIndividualClosure;

static PixbufAvatarFromIndividualClosure *
pixbuf_avatar_from_individual_closure_new (FolksIndividual *individual,
    GSimpleAsyncResult *result,
    gint width,
    gint height,
    GCancellable *cancellable)
{
  PixbufAvatarFromIndividualClosure *closure;

  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (individual), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  closure = g_slice_new0 (PixbufAvatarFromIndividualClosure);
  closure->result = g_object_ref (result);
  closure->width = width;
  closure->height = height;

  if (cancellable != NULL)
    closure->cancellable = g_object_ref (cancellable);

  return closure;
}

static void
pixbuf_avatar_from_individual_closure_free (
    PixbufAvatarFromIndividualClosure *closure)
{
  g_clear_object (&closure->cancellable);
  g_object_unref (closure->result);
  g_slice_free (PixbufAvatarFromIndividualClosure, closure);
}

/**
 * @pixbuf: (transfer all)
 *
 * Return: (transfer all)
 */
static GdkPixbuf *
transform_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *result;

  result = pixbuf_round_corners (pixbuf);
  g_object_unref (pixbuf);

  return result;
}

static void
avatar_icon_load_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  GLoadableIcon *icon = G_LOADABLE_ICON (object);
  PixbufAvatarFromIndividualClosure *closure = user_data;
  GInputStream *stream;
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  GdkPixbuf *final_pixbuf;

  stream = g_loadable_icon_load_finish (icon, result, NULL, &error);
  if (error != NULL)
    {
      DEBUG ("Failed to open avatar stream: %s", error->message);
      g_simple_async_result_set_from_error (closure->result, error);
      goto out;
    }

  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
      closure->width, closure->height, TRUE, closure->cancellable, &error);

  g_object_unref (stream);

  if (pixbuf == NULL)
    {
      DEBUG ("Failed to read avatar: %s", error->message);
      g_simple_async_result_set_from_error (closure->result, error);
      goto out;
    }

  final_pixbuf = transform_pixbuf (pixbuf);

  /* Pass ownership of final_pixbuf to the result */
  g_simple_async_result_set_op_res_gpointer (closure->result,
      final_pixbuf, g_object_unref);

 out:
  g_simple_async_result_complete (closure->result);

  g_clear_error (&error);
  pixbuf_avatar_from_individual_closure_free (closure);
}

const gchar *
empathy_icon_name_for_presence (TpConnectionPresenceType presence)
{
  switch (presence)
    {
    case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
      return EMPATHY_IMAGE_AVAILABLE;
    case TP_CONNECTION_PRESENCE_TYPE_BUSY:
      return EMPATHY_IMAGE_BUSY;
    case TP_CONNECTION_PRESENCE_TYPE_AWAY:
      return EMPATHY_IMAGE_AWAY;
    case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
      if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (),
              EMPATHY_IMAGE_EXT_AWAY))
        return EMPATHY_IMAGE_EXT_AWAY;

      /* The 'extended-away' icon is not an official one so we fallback to
       * idle if it's not implemented */
      return EMPATHY_IMAGE_IDLE;
    case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
      if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (),
              EMPATHY_IMAGE_HIDDEN))
        return EMPATHY_IMAGE_HIDDEN;

      /* The 'hidden' icon is not an official one so we fallback to offline if
       * it's not implemented */
      return EMPATHY_IMAGE_OFFLINE;
    case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
    case TP_CONNECTION_PRESENCE_TYPE_ERROR:
      return EMPATHY_IMAGE_OFFLINE;
    case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
      return EMPATHY_IMAGE_PENDING;
    case TP_CONNECTION_PRESENCE_TYPE_UNSET:
    default:
      return NULL;
    }

  return NULL;
}

const gchar *
empathy_icon_name_for_individual (FolksIndividual *individual)
{
  FolksPresenceType folks_presence;
  TpConnectionPresenceType presence;

  folks_presence = folks_presence_details_get_presence_type (
      FOLKS_PRESENCE_DETAILS (individual));
  presence = empathy_folks_presence_type_to_tp (folks_presence);

  return empathy_icon_name_for_presence (presence);
}

void
empathy_pixbuf_avatar_from_individual_scaled_async (
    FolksIndividual *individual,
    gint width,
    gint height,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GLoadableIcon *avatar_icon;
  GSimpleAsyncResult *result;
  PixbufAvatarFromIndividualClosure *closure;

  result = g_simple_async_result_new (G_OBJECT (individual),
      callback, user_data, empathy_pixbuf_avatar_from_individual_scaled_async);

  avatar_icon = folks_avatar_details_get_avatar (
      FOLKS_AVATAR_DETAILS (individual));

  if (avatar_icon == NULL)
    {
      g_simple_async_result_set_error (result, G_IO_ERROR,
          G_IO_ERROR_NOT_FOUND, "no avatar found");

      g_simple_async_result_complete (result);
      g_object_unref (result);
      return;
    }

  closure = pixbuf_avatar_from_individual_closure_new (individual, result,
      width, height, cancellable);

  g_return_if_fail (closure != NULL);

  g_loadable_icon_load_async (avatar_icon, width, cancellable,
      avatar_icon_load_cb, closure);

  g_object_unref (result);
}


/* Return a ref on the GdkPixbuf */
GdkPixbuf *
empathy_pixbuf_avatar_from_individual_scaled_finish (
    FolksIndividual *individual,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  gboolean result_valid;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (individual), NULL);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  result_valid = g_simple_async_result_is_valid (result,
      G_OBJECT (individual),
      empathy_pixbuf_avatar_from_individual_scaled_async);

  g_return_val_if_fail (result_valid, NULL);

  pixbuf = g_simple_async_result_get_op_res_gpointer (simple);
  return pixbuf != NULL ? g_object_ref (pixbuf) : NULL;
}

GdkPixbuf *
empathy_pixbuf_from_icon_name_sized (const gchar *icon_name,
    gint size)
{
  GtkIconTheme *theme;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!icon_name)
    return NULL;

  theme = gtk_icon_theme_get_default ();

  pixbuf = gtk_icon_theme_load_icon (theme, icon_name, size, 0, &error);

  if (error)
    {
      DEBUG ("Error loading icon: %s", error->message);
      g_clear_error (&error);
    }

  return pixbuf;
}
