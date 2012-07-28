/*
 * empathy-user-info.c - Source for EmpathyUserInfo
 *
 * Copyright (C) 2012 - Collabora Ltd.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with This library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "empathy-user-info.h"

#include <glib/gi18n-lib.h>

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-time.h>
#include <libempathy/empathy-utils.h>

#include "empathy-avatar-chooser.h"
#include "empathy-calendar-button.h"
#include "empathy-contactinfo-utils.h"

G_DEFINE_TYPE (EmpathyUserInfo, empathy_user_info, GTK_TYPE_GRID)

struct _EmpathyUserInfoPrivate
{
  TpAccount *account;

  GtkWidget *avatar_chooser;
  GtkWidget *nickname_entry;
  GtkWidget *details_label;
  GtkWidget *details_spinner;

  GList *details_to_set;
  gboolean details_changed;
  GCancellable *details_cancellable;
};

enum
{
  PROP_0,
  PROP_ACCOUNT,
};

#define DATA_FIELD "contact-info-field"
#define DATA_IS_CONTACT_INFO "is-contact-info"

static void
contact_info_changed_cb (GtkEntry *entry,
    EmpathyUserInfo *self)
{
  const gchar *strv[] = { NULL, NULL };
  TpContactInfoField *field;

  self->priv->details_changed = TRUE;

  field = g_object_get_data ((GObject *) entry, DATA_FIELD);
  g_assert (field != NULL);

  strv[0] = gtk_entry_get_text (entry);

  if (field->field_value != NULL)
    g_strfreev (field->field_value);
  field->field_value = g_strdupv ((GStrv) strv);
}

static void
bday_changed_cb (EmpathyCalendarButton *button,
    GDate *date,
    EmpathyUserInfo *self)
{
  const gchar *strv[] = { NULL, NULL };
  TpContactInfoField *field;

  self->priv->details_changed = TRUE;

  field = g_object_get_data ((GObject *) button, DATA_FIELD);
  g_assert (field != NULL);

  if (date != NULL)
    {
      gchar tmp[255];

      g_date_strftime (tmp, sizeof (tmp), EMPATHY_DATE_FORMAT_DISPLAY_SHORT,
          date);
      strv[0] = tmp;
    }

  if (field->field_value != NULL)
    g_strfreev (field->field_value);
  field->field_value = g_strdupv ((GStrv) strv);
}

static gboolean
field_name_in_field_list (GList *list,
    const gchar *name)
{
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      TpContactInfoField *field = l->data;

      if (!tp_strdiff (field->field_name, name))
        return TRUE;
    }

  return FALSE;
}

static TpContactInfoFieldSpec *
get_spec_from_list (GList *list,
    const gchar *name)
{
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      TpContactInfoFieldSpec *spec = l->data;

      if (!tp_strdiff (spec->name, name))
        return spec;
    }

  return NULL;
}

static void
add_row (GtkGrid *grid,
    GtkWidget *title,
    GtkWidget *value,
    gboolean contact_info)
{
  /* Title */
  gtk_grid_attach_next_to (grid, title, NULL, GTK_POS_BOTTOM, 1, 1);
  gtk_misc_set_alignment (GTK_MISC (title), 1, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (title),
      GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_show (title);

  /* Value */
  gtk_grid_attach_next_to (grid, value, title, GTK_POS_RIGHT,
      contact_info ? 2 : 1, 1);
  gtk_widget_set_hexpand (value, TRUE);
  if (GTK_IS_LABEL (value))
    {
      gtk_misc_set_alignment (GTK_MISC (value), 0, 0.5);
      gtk_label_set_selectable (GTK_LABEL (value), TRUE);
    }
  gtk_widget_show (value);

  if (contact_info)
    {
      g_object_set_data (G_OBJECT (title),
          DATA_IS_CONTACT_INFO, (gpointer) TRUE);
      g_object_set_data (G_OBJECT (value),
          DATA_IS_CONTACT_INFO, (gpointer) TRUE);
    }
}

static guint
fill_contact_info_grid (EmpathyUserInfo *self)
{
  TpConnection *connection;
  TpContact *contact;
  GList *specs, *l;
  guint n_rows = 0;
  GList *info;
  const char **field_names = empathy_contact_info_get_field_names (NULL);
  guint i;

  g_assert (self->priv->details_to_set == NULL);

  connection = tp_account_get_connection (self->priv->account);
  contact = tp_connection_get_self_contact (connection);
  specs = tp_connection_get_contact_info_supported_fields (connection);
  info = tp_contact_get_contact_info (contact);

  /* Look at the fields set in our vCard */
  for (l = info; l != NULL; l = l->next)
    {
      TpContactInfoField *field = l->data;

      /* make a copy for the details_to_set list */
      field = tp_contact_info_field_copy (field);
      DEBUG ("Field %s is in our vCard", field->field_name);

      self->priv->details_to_set = g_list_prepend (self->priv->details_to_set,
          field);
    }

  /* Add fields which are supported but not in the vCard */
  for (i = 0; field_names[i] != NULL; i++)
    {
      TpContactInfoFieldSpec *spec;
      TpContactInfoField *field;

      /* Check if the field was in the vCard */
      if (field_name_in_field_list (self->priv->details_to_set,
            field_names[i]))
        continue;

      /* Check if the CM supports the field */
      spec = get_spec_from_list (specs, field_names[i]);
      if (spec == NULL)
        continue;

      /* add an empty field so user can set a value */
      field = tp_contact_info_field_new (spec->name, spec->parameters, NULL);

      self->priv->details_to_set = g_list_prepend (self->priv->details_to_set,
          field);
    }

  /* Add widgets for supported fields */
  self->priv->details_to_set = g_list_sort (self->priv->details_to_set,
      (GCompareFunc) empathy_contact_info_field_spec_cmp);

  for (l = self->priv->details_to_set; l != NULL; l= g_list_next (l))
    {
      TpContactInfoField *field = l->data;
      GtkWidget *label, *w;
      TpContactInfoFieldSpec *spec;
      gboolean has_field;
      char *title;

      has_field = empathy_contact_info_lookup_field (field->field_name,
          NULL, NULL);
      if (!has_field)
        {
          /* Empathy doesn't display this field so we can't change it.
           * But we put it in the details_to_set list so it won't be erased
           * when calling SetContactInfo (bgo #630427) */
          DEBUG ("Unhandled ContactInfo field spec: %s", field->field_name);
          continue;
        }

      spec = get_spec_from_list (specs, field->field_name);
      /* We shouldn't have added the field to details_to_set if it's not
       * supported by the CM */
      g_assert (spec != NULL);

      if (spec->flags & TP_CONTACT_INFO_FIELD_FLAG_OVERWRITTEN_BY_NICKNAME)
        {
          DEBUG ("Ignoring field '%s' due it to having the "
              "Overwritten_By_Nickname flag", field->field_name);
          continue;
        }

      /* Add Title */
      title = empathy_contact_info_field_label (field->field_name,
          field->parameters,
          (spec->flags & TP_CONTACT_INFO_FIELD_FLAG_PARAMETERS_EXACT));
      label = gtk_label_new (title);
      g_free (title);

      /* TODO: if TP_CONTACT_INFO_FIELD_FLAG_PARAMETERS_EXACT is not set we
       * should allow user to tag the vCard fields (bgo#672034) */

      /* Add Value */
      if (!tp_strdiff (field->field_name, "bday"))
        {
          w = empathy_calendar_button_new ();

          if (field->field_value[0])
            {
              GDate date;

              g_date_set_parse (&date, field->field_value[0]);
              if (g_date_valid (&date))
                {
                  empathy_calendar_button_set_date (EMPATHY_CALENDAR_BUTTON (w),
                      &date);
                }
            }

          g_signal_connect (w, "date-changed",
            G_CALLBACK (bday_changed_cb), self);
        }
      else
        {
          w = gtk_entry_new ();
          gtk_entry_set_text (GTK_ENTRY (w),
              field->field_value[0] ? field->field_value[0] : "");
          g_signal_connect (w, "changed",
            G_CALLBACK (contact_info_changed_cb), self);
        }

      add_row (GTK_GRID (self), label, w, TRUE);

      g_object_set_data ((GObject *) w, DATA_FIELD, field);

      n_rows++;
    }

  g_list_free (specs);
  g_list_free (info);

  return n_rows;
}

static void
grid_foreach_cb (GtkWidget *widget,
    gpointer data)
{
  if (g_object_get_data (G_OBJECT (widget), DATA_IS_CONTACT_INFO) != NULL)
    gtk_widget_destroy (widget);
}

static void
request_contact_info_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  EmpathyUserInfo *self = user_data;
  TpContact *contact = TP_CONTACT (object);
  guint n_rows;
  GError *error = NULL;

  if (!tp_contact_request_contact_info_finish (contact, res, &error))
    {
      /* If the request got cancelled it could mean the contact widget is
       * destroyed, so we should not dereference self */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_clear_error (&error);
          return;
        }
      g_clear_error (&error);
    }

  n_rows = fill_contact_info_grid (self);

  gtk_widget_set_visible (self->priv->details_label, n_rows > 0);
  gtk_spinner_stop (GTK_SPINNER (self->priv->details_spinner));
  gtk_widget_hide (self->priv->details_spinner);
}

static void
reload_contact_info (EmpathyUserInfo *self)
{
  TpConnection *connection;
  TpContact *contact = NULL;
  TpContactInfoFlags flags;

  /* Cancel previous RequestContactInfo, if any */
  if (self->priv->details_cancellable != NULL)
    g_cancellable_cancel (self->priv->details_cancellable);
  g_clear_object (&self->priv->details_cancellable);

  /* Remove current contact info widgets, if any */
  gtk_container_foreach (GTK_CONTAINER (self), grid_foreach_cb, NULL);
  gtk_widget_hide (self->priv->details_label);
  gtk_widget_hide (self->priv->details_spinner);

  tp_clear_pointer (&self->priv->details_to_set, tp_contact_info_list_free);
  self->priv->details_changed = FALSE;

  connection = tp_account_get_connection (self->priv->account);
  if (connection != NULL)
    contact = tp_connection_get_self_contact (connection);

  /* Display infobar if we don't have a self contact (probably offline) */
  if (contact == NULL)
    {
      GtkWidget *infobar;
      GtkWidget *content;
      GtkWidget *label;

      infobar = gtk_info_bar_new ();
      gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_INFO);
      content = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
      label = gtk_label_new (_("Go online to edit your personal information."));
      gtk_container_add (GTK_CONTAINER (content), label);
      gtk_widget_show (label);

      gtk_grid_attach_next_to ((GtkGrid *) self, infobar,
          NULL, GTK_POS_BOTTOM, 3, 1);
      gtk_widget_show (infobar);

      g_object_set_data (G_OBJECT (infobar),
          DATA_IS_CONTACT_INFO, (gpointer) TRUE);
      return;
    }

  if (!tp_proxy_has_interface_by_id (connection,
          TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACT_INFO))
    return;

  flags = tp_connection_get_contact_info_flags (connection);
  if ((flags & TP_CONTACT_INFO_FLAG_CAN_SET) == 0)
    return;

  /* Request the contact's info */
  gtk_widget_show (self->priv->details_spinner);
  gtk_spinner_start (GTK_SPINNER (self->priv->details_spinner));

  g_assert (self->priv->details_cancellable == NULL);
  self->priv->details_cancellable = g_cancellable_new ();
  tp_contact_request_contact_info_async (contact,
      self->priv->details_cancellable, request_contact_info_cb,
      self);
}

static void
connection_notify_cb (EmpathyUserInfo *self)
{
  TpConnection *connection = tp_account_get_connection (self->priv->account);

  if (connection != NULL)
    {
      tp_g_signal_connect_object (connection, "notify::self-contact",
          G_CALLBACK (reload_contact_info), self, G_CONNECT_SWAPPED);
    }

  reload_contact_info (self);
}

static void
empathy_user_info_constructed (GObject *object)
{
  EmpathyUserInfo *self = (EmpathyUserInfo *) object;
  GtkGrid *grid = (GtkGrid *) self;
  GtkWidget *title;
  GtkWidget *value;

  G_OBJECT_CLASS (empathy_user_info_parent_class)->constructed (object);

  gtk_grid_set_column_spacing (grid, 6);
  gtk_grid_set_row_spacing (grid, 6);

  /* Setup id label */
  title = gtk_label_new (_("Identifier"));
  value = gtk_label_new (tp_account_get_normalized_name (self->priv->account));
  add_row (grid, title, value, FALSE);

  /* Setup nickname entry */
  title = gtk_label_new (_("Alias"));
  self->priv->nickname_entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (self->priv->nickname_entry),
      tp_account_get_nickname (self->priv->account));
  add_row (grid, title, self->priv->nickname_entry, FALSE);

  /* Set up avatar chooser */
  self->priv->avatar_chooser = empathy_avatar_chooser_new (self->priv->account);
  gtk_grid_attach (grid, self->priv->avatar_chooser,
      2, 0, 1, 3);
  gtk_widget_show (self->priv->avatar_chooser);

  /* Details label */
  self->priv->details_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (self->priv->details_label),
      _("<b>Personal Details</b>"));
  gtk_misc_set_alignment (GTK_MISC (self->priv->details_label), 0, 0.5);
  gtk_grid_attach_next_to (grid, self->priv->details_label, NULL,
      GTK_POS_BOTTOM, 3, 1);

  /* Details spinner */
  self->priv->details_spinner = gtk_spinner_new ();
  gtk_widget_set_hexpand (self->priv->details_spinner, TRUE);
  gtk_widget_set_vexpand (self->priv->details_spinner, TRUE);
  gtk_grid_attach_next_to (grid, self->priv->details_spinner, NULL,
      GTK_POS_BOTTOM, 3, 1);

  g_signal_connect_swapped (self->priv->account, "notify::connection",
      G_CALLBACK (connection_notify_cb), self);
  connection_notify_cb (self);
}

static void
empathy_user_info_dispose (GObject *object)
{
  EmpathyUserInfo *self = (EmpathyUserInfo *) object;

  if (self->priv->account != NULL)
    {
      /* Disconnect the signal manually, because TpAccount::dispose will emit
       * "notify::connection" signal before tp_g_signal_connect_object() had
       * a chance to disconnect. */
      g_signal_handlers_disconnect_by_func (self->priv->account,
          connection_notify_cb, self);
      g_clear_object (&self->priv->account);
    }

  if (self->priv->details_cancellable != NULL)
    g_cancellable_cancel (self->priv->details_cancellable);
  g_clear_object (&self->priv->details_cancellable);

  G_OBJECT_CLASS (empathy_user_info_parent_class)->dispose (object);
}

static void
empathy_user_info_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyUserInfo *self = (EmpathyUserInfo *) object;

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, self->priv->account);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_user_info_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyUserInfo *self = (EmpathyUserInfo *) object;

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_assert (self->priv->account == NULL); /* construct-only */
        self->priv->account = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_user_info_init (EmpathyUserInfo *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_USER_INFO, EmpathyUserInfoPrivate);
}

static void
empathy_user_info_class_init (EmpathyUserInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->constructed = empathy_user_info_constructed;
  object_class->dispose = empathy_user_info_dispose;
  object_class->get_property = empathy_user_info_get_property;
  object_class->set_property = empathy_user_info_set_property;

  g_type_class_add_private (object_class, sizeof (EmpathyUserInfoPrivate));

  param_spec = g_param_spec_object ("account",
      "account",
      "The #TpAccount on which user info should be edited",
      TP_TYPE_ACCOUNT,
      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);
}

GtkWidget *
empathy_user_info_new (TpAccount *account)
{
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  return g_object_new (EMPATHY_TYPE_USER_INFO,
      "account", account,
      NULL);
}

void
empathy_user_info_discard (EmpathyUserInfo *self)
{
  g_return_if_fail (EMPATHY_IS_USER_INFO (self));

  reload_contact_info (self);
  gtk_entry_set_text ((GtkEntry *) self->priv->nickname_entry,
      tp_account_get_nickname (self->priv->account));
}

static void
apply_complete_one (GSimpleAsyncResult *result)
{
  guint count;

  count = g_simple_async_result_get_op_res_gssize (result);
  count--;
  g_simple_async_result_set_op_res_gssize (result, count);

  if (count == 0)
    g_simple_async_result_complete (result);
}

static void
avatar_chooser_apply_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  EmpathyAvatarChooser *avatar_chooser = (EmpathyAvatarChooser *) source;
  GSimpleAsyncResult *my_result = user_data;
  GError *error = NULL;

  if (!empathy_avatar_chooser_apply_finish (avatar_chooser, result, &error))
    g_simple_async_result_take_error (my_result, error);

  apply_complete_one (my_result);
  g_object_unref (my_result);
}

static void
set_nickname_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpAccount *account = (TpAccount *) source;
  GSimpleAsyncResult *my_result = user_data;
  GError *error = NULL;

  if (!tp_account_set_nickname_finish (account, result, &error))
    g_simple_async_result_take_error (my_result, error);

  apply_complete_one (my_result);
  g_object_unref (my_result);
}

static void
set_contact_info_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  GSimpleAsyncResult *my_result = user_data;
  GError *error = NULL;

  if (!tp_connection_set_contact_info_finish (connection, result, &error))
    g_simple_async_result_take_error (my_result, error);

  apply_complete_one (my_result);
  g_object_unref (my_result);
}

static gboolean
field_value_is_empty (TpContactInfoField *field)
{
  guint i;

  if (field->field_value == NULL)
    return TRUE;

  /* Field is empty if all its values are empty */
  for (i = 0; field->field_value[i] != NULL; i++)
    {
      if (!tp_str_empty (field->field_value[i]))
        return FALSE;
    }

  return TRUE;
}

void
empathy_user_info_apply_async (EmpathyUserInfo *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  const gchar *new_nickname;
  guint count = 0;
  GList *l, *next;

  g_return_if_fail (EMPATHY_IS_USER_INFO (self));

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      empathy_user_info_apply_async);

  /* Apply avatar */
  empathy_avatar_chooser_apply_async (
      (EmpathyAvatarChooser *) self->priv->avatar_chooser,
      avatar_chooser_apply_cb, g_object_ref (result));
  count++;

  /* Apply nickname */
  new_nickname = gtk_entry_get_text (GTK_ENTRY (self->priv->nickname_entry));
  if (tp_strdiff (new_nickname, tp_account_get_nickname (self->priv->account)))
    {
      tp_account_set_nickname_async (self->priv->account, new_nickname,
          set_nickname_cb, g_object_ref (result));
      count++;
    }

  /* Remove empty fields */
  for (l = self->priv->details_to_set; l != NULL; l = next)
    {
      TpContactInfoField *field = l->data;

      next = l->next;
      if (field_value_is_empty (field))
        {
          DEBUG ("Drop empty field: %s", field->field_name);
          tp_contact_info_field_free (field);
          self->priv->details_to_set =
              g_list_delete_link (self->priv->details_to_set, l);
        }
    }

  if (self->priv->details_to_set != NULL)
    {
      if (self->priv->details_changed)
        {
          tp_connection_set_contact_info_async (
              tp_account_get_connection (self->priv->account),
              self->priv->details_to_set, set_contact_info_cb,
              g_object_ref (result));
          count++;
        }

      tp_contact_info_list_free (self->priv->details_to_set);
      self->priv->details_to_set = NULL;
    }

  self->priv->details_changed = FALSE;

  g_simple_async_result_set_op_res_gssize (result, count);

  g_object_unref (result);
}

gboolean
empathy_user_info_apply_finish (EmpathyUserInfo *self,
    GAsyncResult *result,
    GError **error)
{
  empathy_implement_finish_void (self, empathy_user_info_apply_async);
}
