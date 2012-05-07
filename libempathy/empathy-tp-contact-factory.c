/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
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
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include "empathy-tp-contact-factory.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP | EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

static TpContactFeature contact_features[] = {
	TP_CONTACT_FEATURE_ALIAS,
	TP_CONTACT_FEATURE_AVATAR_DATA,
	TP_CONTACT_FEATURE_PRESENCE,
	TP_CONTACT_FEATURE_LOCATION,
	TP_CONTACT_FEATURE_CAPABILITIES,
	TP_CONTACT_FEATURE_CLIENT_TYPES,
};

typedef union {
	EmpathyTpContactFactoryContactCb contact_cb;
} GetContactsCb;

typedef struct {
        TpConnection *connection;
	GetContactsCb callback;
	gpointer user_data;
	GDestroyNotify destroy;
} GetContactsData;

static void
get_contacts_data_free (gpointer user_data)
{
	GetContactsData *data = user_data;

	if (data->destroy) {
		data->destroy (data->user_data);
	}
	g_object_unref (data->connection);

	g_slice_free (GetContactsData, data);
}

static void
get_contact_by_id_cb (TpConnection *connection,
		      guint n_contacts,
		      TpContact * const *contacts,
		      const gchar * const *requested_ids,
		      GHashTable *failed_id_errors,
		      const GError *error,
		      gpointer user_data,
		      GObject *weak_object)
{
	GetContactsData *data = user_data;
	EmpathyContact  *contact = NULL;

	if (n_contacts == 1) {
		contact = empathy_contact_dup_from_tp_contact (contacts[0]);
	}
	else if (error == NULL) {
		GHashTableIter iter;
		gpointer       value;

		g_hash_table_iter_init (&iter, failed_id_errors);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			if (value) {
				error = value;
				break;
			}
		}
	}

	if (data->callback.contact_cb) {
		data->callback.contact_cb (data->connection,
				           contact,
				           error,
				           data->user_data, weak_object);
	}

	if (contact != NULL)
		g_object_unref (contact);
}

/* The callback is NOT given a reference to the EmpathyContact objects */
void
empathy_tp_contact_factory_get_from_id (TpConnection            *connection,
					const gchar             *id,
					EmpathyTpContactFactoryContactCb callback,
					gpointer                 user_data,
					GDestroyNotify           destroy,
					GObject                 *weak_object)
{
	GetContactsData *data;

	g_return_if_fail (TP_IS_CONNECTION (connection));
	g_return_if_fail (id != NULL);

	data = g_slice_new (GetContactsData);
	data->callback.contact_cb = callback;
	data->user_data = user_data;
	data->destroy = destroy;
	data->connection = g_object_ref (connection);
	tp_connection_get_contacts_by_id (connection,
					  1, &id,
					  G_N_ELEMENTS (contact_features),
					  contact_features,
					  get_contact_by_id_cb,
					  data,
					  (GDestroyNotify) get_contacts_data_free,
					  weak_object);
}

/* The callback is NOT given a reference to the EmpathyContact objects */
static void
get_contact_by_handle_cb (TpConnection *connection,
			  guint n_contacts,
			  TpContact * const *contacts,
			  guint n_failed,
			  const TpHandle *failed,
			  const GError *error,
			  gpointer user_data,
			  GObject *weak_object)
{
	GetContactsData *data = user_data;
	EmpathyContact  *contact = NULL;
	GError *err = NULL;

	if (n_contacts == 1) {
		contact = empathy_contact_dup_from_tp_contact (contacts[0]);
	}
	else {
		if (error == NULL) {
			/* tp-glib will provide an error only if the whole operation failed,
			 * but not if, for example, the handle was invalid. We create an error
			 * so the caller of empathy_tp_contact_factory_get_from_handle can
			 * rely on the error to check if the operation succeeded or not. */

			err = g_error_new_literal (TP_ERROR, TP_ERROR_INVALID_HANDLE,
						      "handle is invalid");
		}
		else {
			err = g_error_copy (error);
		}
	}

	if (data->callback.contact_cb) {
		data->callback.contact_cb (data->connection,
				           contact,
				           err,
				           data->user_data, weak_object);
	}

	g_clear_error (&err);
	if (contact != NULL)
		g_object_unref (contact);
}

void
empathy_tp_contact_factory_get_from_handle (TpConnection            *connection,
					    TpHandle                 handle,
					    EmpathyTpContactFactoryContactCb callback,
					    gpointer                 user_data,
					    GDestroyNotify           destroy,
					    GObject                 *weak_object)
{
	GetContactsData *data;

	g_return_if_fail (TP_IS_CONNECTION (connection));

	data = g_slice_new (GetContactsData);
	data->callback.contact_cb = callback;
	data->user_data = user_data;
	data->destroy = destroy;
	data->connection = g_object_ref (connection);
	tp_connection_get_contacts_by_handle (connection,
					      1, &handle,
					      G_N_ELEMENTS (contact_features),
					      contact_features,
					      get_contact_by_handle_cb,
					      data,
					      (GDestroyNotify) get_contacts_data_free,
					      weak_object);
}

