/*
 * Copyright (C) 2010 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General  License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General  License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "mock-pkcs11.h"

#include <gcr/gcr.h>

#include <glib.h>

#include <string.h>

/*
 * This is *NOT* how you'd want to implement a PKCS#11 module. This
 * fake module simply provides enough for gnutls-pkcs11 backend to test against.
 * It doesn't pass any tests, or behave as expected from a PKCS#11 module.
 */

static gboolean initialized = FALSE;

typedef enum {
  OP_FIND = 1,
} Operation;

static CK_OBJECT_HANDLE unique_identifier = 100;
static GHashTable *the_sessions = NULL;
static GHashTable *the_certificates = NULL;
static GHashTable *the_assertions = NULL;

typedef struct {
  GcrCertificate *cert;
  CK_ULONG assertion_type;
  gchar *purpose;
  gchar *peer;
} Assertion;

static void
free_assertion (gpointer data)
{
  Assertion *assertion = data;
  g_clear_object (&assertion->cert);
  g_free (assertion->purpose);
  g_free (assertion->peer);
  g_free (assertion);
}

typedef struct {
  CK_SESSION_HANDLE handle;
  CK_SESSION_INFO info;

  Operation operation;

  /* For find operations */
  GList *matches;
} Session;

static void
free_session (gpointer data)
{
  Session *sess = (Session*)data;
  g_list_free (sess->matches);
  g_free (sess);
}

CK_OBJECT_HANDLE
mock_module_add_certificate (GcrCertificate *cert)
{
  CK_OBJECT_HANDLE handle;

  g_return_val_if_fail (GCR_IS_CERTIFICATE (cert), 0);

  handle = unique_identifier++;
  g_hash_table_insert (the_certificates, GUINT_TO_POINTER (handle), g_object_ref (cert));
  return handle;
}

CK_OBJECT_HANDLE
mock_module_add_assertion (GcrCertificate *cert,
                           CK_X_ASSERTION_TYPE assertion_type,
                           const gchar *purpose,
                           const gchar *peer)
{
  Assertion *assertion;
  CK_OBJECT_HANDLE handle;

  g_return_val_if_fail (GCR_IS_CERTIFICATE (cert), 0);

  assertion = g_new0 (Assertion, 1);
  assertion->cert = g_object_ref (cert);
  assertion->assertion_type = assertion_type;
  assertion->purpose = g_strdup (purpose);
  assertion->peer = g_strdup (peer);

  handle = unique_identifier++;
  g_hash_table_insert (the_assertions, GUINT_TO_POINTER (handle), assertion);
  return handle;
}

CK_RV
mock_C_Initialize (CK_VOID_PTR init_args)
{
  CK_C_INITIALIZE_ARGS_PTR args;

  g_return_val_if_fail (initialized == FALSE, CKR_CRYPTOKI_ALREADY_INITIALIZED);

  args = (CK_C_INITIALIZE_ARGS_PTR)init_args;
  if (args)
    {
      g_return_val_if_fail(
          (args->CreateMutex == NULL && args->DestroyMutex == NULL &&
           args->LockMutex == NULL && args->UnlockMutex == NULL) ||
          (args->CreateMutex != NULL && args->DestroyMutex != NULL &&
           args->LockMutex != NULL && args->UnlockMutex != NULL),
          CKR_ARGUMENTS_BAD);

      /* Flags should allow OS locking and os threads */
      g_return_val_if_fail ((args->flags & CKF_OS_LOCKING_OK), CKR_CANT_LOCK);
      g_return_val_if_fail ((args->flags & CKF_LIBRARY_CANT_CREATE_OS_THREADS) == 0, CKR_NEED_TO_CREATE_THREADS);
    }

  the_sessions = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_session);
  the_certificates = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_object_unref);
  the_assertions = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_assertion);

  initialized = TRUE;
  return CKR_OK;
}

CK_RV
mock_C_Finalize (CK_VOID_PTR reserved)
{
  g_return_val_if_fail (reserved == NULL, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail (initialized == TRUE, CKR_CRYPTOKI_NOT_INITIALIZED);

  initialized = FALSE;

  g_hash_table_destroy (the_certificates);
  the_certificates = NULL;

  g_hash_table_destroy (the_assertions);
  the_assertions = NULL;

  g_hash_table_destroy (the_sessions);
  the_sessions = NULL;

  return CKR_OK;
}

static const CK_INFO TEST_INFO = {
  { CRYPTOKI_VERSION_MAJOR, CRYPTOKI_VERSION_MINOR },
  "TEST MANUFACTURER              ",
  0,
  "TEST LIBRARY                   ",
  { 45, 145 }
};

CK_RV
mock_C_GetInfo (CK_INFO_PTR info)
{
  g_return_val_if_fail (info, CKR_ARGUMENTS_BAD);
  memcpy (info, &TEST_INFO, sizeof (*info));
  return CKR_OK;
}

CK_RV
mock_C_GetFunctionList (CK_FUNCTION_LIST_PTR_PTR list)
{
  g_return_val_if_fail (list, CKR_ARGUMENTS_BAD);
  *list = &mock_default_functions;
  return CKR_OK;
}

CK_RV
mock_C_GetSlotList (CK_BBOOL token_present,
                    CK_SLOT_ID_PTR slot_list,
                    CK_ULONG_PTR count)
{
  CK_ULONG num = 1;

  g_return_val_if_fail (count, CKR_ARGUMENTS_BAD);

  /* Application only wants to know the number of slots. */
  if (slot_list == NULL)
    {
      *count = num;
      return CKR_OK;
    }

  if (*count < num)
    g_return_val_if_reached (CKR_BUFFER_TOO_SMALL);

  *count = num;
  slot_list[0] = MOCK_SLOT_ONE_ID;
  return CKR_OK;
}

/* Update mock-pkcs11.h URIs when updating this */

static const CK_SLOT_INFO MOCK_INFO_ONE = {
  "MOCK SLOT                                                       ",
  "MOCK MANUFACTURER              ",
  CKF_TOKEN_PRESENT | CKF_REMOVABLE_DEVICE,
  { 55, 155 },
  { 65, 165 },
};

CK_RV
mock_C_GetSlotInfo (CK_SLOT_ID slot_id,
                    CK_SLOT_INFO_PTR info)
{
  g_return_val_if_fail (info, CKR_ARGUMENTS_BAD);

  if (slot_id == MOCK_SLOT_ONE_ID)
    {
      memcpy (info, &MOCK_INFO_ONE, sizeof (*info));
      return CKR_OK;
    }
  else
    {
      g_return_val_if_reached (CKR_SLOT_ID_INVALID);
    }
}

/* Update mock-pkcs11.h URIs when updating this */

static const CK_TOKEN_INFO MOCK_TOKEN_ONE = {
  "MOCK LABEL                      ",
  "MOCK MANUFACTURER               ",
  "MOCK MODEL      ",
  "MOCK SERIAL     ",
  CKF_TOKEN_INITIALIZED | CKF_WRITE_PROTECTED,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  { 75, 175 },
  { 85, 185 },
  { '1', '9', '9', '9', '0', '5', '2', '5', '0', '9', '1', '9', '5', '9', '0', '0' }
};

CK_RV
mock_C_GetTokenInfo (CK_SLOT_ID slot_id,
                     CK_TOKEN_INFO_PTR info)
{
  g_return_val_if_fail (info != NULL, CKR_ARGUMENTS_BAD);

  if (slot_id == MOCK_SLOT_ONE_ID)
    {
      memcpy (info, &MOCK_TOKEN_ONE, sizeof (*info));
      return CKR_OK;
    }
  else
    {
      g_return_val_if_reached (CKR_SLOT_ID_INVALID);
    }
}

CK_RV
mock_C_GetMechanismList (CK_SLOT_ID slot_id,
                         CK_MECHANISM_TYPE_PTR mechanism_list,
                         CK_ULONG_PTR count)
{
  g_return_val_if_fail (slot_id == MOCK_SLOT_ONE_ID, CKR_SLOT_ID_INVALID);
  g_return_val_if_fail (count, CKR_ARGUMENTS_BAD);

  /* Application only wants to know the number of slots. */
  if (mechanism_list == NULL)
    {
      *count = 0;
      return CKR_OK;
    }

  return CKR_OK;
}

CK_RV
mock_C_GetMechanismInfo (CK_SLOT_ID slot_id,
                         CK_MECHANISM_TYPE type,
                         CK_MECHANISM_INFO_PTR info)
{
  g_return_val_if_fail (slot_id == MOCK_SLOT_ONE_ID, CKR_SLOT_ID_INVALID);
  g_return_val_if_fail (info, CKR_ARGUMENTS_BAD);

  g_return_val_if_reached (CKR_MECHANISM_INVALID);
}

CK_RV
mock_unsupported_C_InitToken (CK_SLOT_ID slot_id,
                              CK_UTF8CHAR_PTR pin,
                              CK_ULONG pin_len,
                              CK_UTF8CHAR_PTR label)
{
  g_return_val_if_fail (slot_id == MOCK_SLOT_ONE_ID, CKR_SLOT_ID_INVALID);
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_WaitForSlotEvent (CK_FLAGS flags,
                                     CK_SLOT_ID_PTR slot_id,
                                     CK_VOID_PTR reserved)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_C_OpenSession (CK_SLOT_ID slot_id,
                    CK_FLAGS flags,
                    CK_VOID_PTR application,
                    CK_NOTIFY notify,
                    CK_SESSION_HANDLE_PTR session)
{
  Session *sess;

  g_return_val_if_fail (slot_id == MOCK_SLOT_ONE_ID, CKR_SLOT_ID_INVALID);
  g_return_val_if_fail (session != NULL, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail ((flags & CKF_SERIAL_SESSION) == CKF_SERIAL_SESSION, CKR_SESSION_PARALLEL_NOT_SUPPORTED);

  if (flags & CKF_RW_SESSION)
    return CKR_TOKEN_WRITE_PROTECTED;

  sess = g_new0 (Session, 1);
  sess->handle = ++unique_identifier;
  sess->info.flags = flags;
  sess->info.slotID = slot_id;
  sess->info.state = CKS_RO_PUBLIC_SESSION;
  sess->info.ulDeviceError = 0;
  *session = sess->handle;

  g_hash_table_replace (the_sessions, GUINT_TO_POINTER (sess->handle), sess);
  return CKR_OK;
}

CK_RV
mock_C_CloseSession (CK_SESSION_HANDLE session)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  g_hash_table_remove (the_sessions, GUINT_TO_POINTER (sess));
  return CKR_OK;
}

CK_RV
mock_C_CloseAllSessions (CK_SLOT_ID slot_id)
{
  g_return_val_if_fail (slot_id == MOCK_SLOT_ONE_ID, CKR_SLOT_ID_INVALID);

  g_hash_table_remove_all (the_sessions);
  return CKR_OK;
}

CK_RV
mock_C_GetFunctionStatus (CK_SESSION_HANDLE session)
{
  return CKR_FUNCTION_NOT_PARALLEL;
}

CK_RV
mock_C_CancelFunction (CK_SESSION_HANDLE session)
{
  return CKR_FUNCTION_NOT_PARALLEL;
}

CK_RV
mock_C_GetSessionInfo (CK_SESSION_HANDLE session,
                       CK_SESSION_INFO_PTR info)
{
  Session *sess;

  g_return_val_if_fail (info != NULL, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  memcpy (info, &sess->info, sizeof (*info));
  return CKR_OK;
}

CK_RV
mock_unsupported_C_InitPIN (CK_SESSION_HANDLE session,
                            CK_UTF8CHAR_PTR pin,
                            CK_ULONG pin_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SetPIN (CK_SESSION_HANDLE session,
                           CK_UTF8CHAR_PTR old_pin,
                           CK_ULONG old_len,
                           CK_UTF8CHAR_PTR new_pin,
                           CK_ULONG new_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_GetOperationState (CK_SESSION_HANDLE session,
                                      CK_BYTE_PTR operation_state,
                                      CK_ULONG_PTR operation_state_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SetOperationState (CK_SESSION_HANDLE session,
                                      CK_BYTE_PTR operation_state,
                                      CK_ULONG operation_state_len,
                                      CK_OBJECT_HANDLE encryption_key,
                                      CK_OBJECT_HANDLE authentication_key)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_Login (CK_SESSION_HANDLE session,
                          CK_USER_TYPE user_type,
                          CK_UTF8CHAR_PTR pin,
                          CK_ULONG pin_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_Logout (CK_SESSION_HANDLE session)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_readonly_C_CreateObject (CK_SESSION_HANDLE session,
                              CK_ATTRIBUTE_PTR template,
                              CK_ULONG count,
                              CK_OBJECT_HANDLE_PTR object)
{
  Session *sess;

  g_return_val_if_fail (object, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_TOKEN_WRITE_PROTECTED;
}

CK_RV
mock_unsupported_C_CopyObject (CK_SESSION_HANDLE session,
                               CK_OBJECT_HANDLE object,
                               CK_ATTRIBUTE_PTR template,
                               CK_ULONG count,
                               CK_OBJECT_HANDLE_PTR new_object)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_readonly_C_DestroyObject (CK_SESSION_HANDLE session,
                               CK_OBJECT_HANDLE object)
{
  Session *sess;

  g_return_val_if_fail (object, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_TOKEN_WRITE_PROTECTED;
}

CK_RV
mock_unsupported_C_GetObjectSize (CK_SESSION_HANDLE session,
                                  CK_OBJECT_HANDLE object,
                                  CK_ULONG_PTR pulSize)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

static CK_RV
fill_data_attribute (CK_ATTRIBUTE *attr,
                     gconstpointer data,
                     gsize length)
{
  if (!attr->pValue) {
      attr->ulValueLen = length;
      return CKR_OK;
  } else if (attr->ulValueLen < length) {
      attr->ulValueLen = length;
      return CKR_BUFFER_TOO_SMALL;
  } else {
      memcpy (attr->pValue, data, length);
      attr->ulValueLen = length;
      return CKR_OK;
  }
}

static CK_RV
fill_check_value_attribute (CK_ATTRIBUTE *attr,
                            GcrCertificate *cert)
{
  guchar *data;
  gsize length;
  CK_RV rv;

  data = gcr_certificate_get_fingerprint (cert, G_CHECKSUM_SHA1, &length);
  rv = fill_data_attribute (attr, data, 3);
  g_free (data);

  return rv;
}

static CK_RV
fill_subject_attribute (CK_ATTRIBUTE *attr,
                        GcrCertificate *cert)
{
  guchar *data;
  gsize length;
  CK_RV rv;

  data = gcr_certificate_get_subject_raw (cert, &length);
  rv = fill_data_attribute (attr, data, length);
  g_free (data);

  return rv;
}

static CK_RV
fill_issuer_attribute (CK_ATTRIBUTE *attr,
                       GcrCertificate *cert)
{
  guchar *data;
  gsize length;
  CK_RV rv;

  data = gcr_certificate_get_issuer_raw (cert, &length);
  rv = fill_data_attribute (attr, data, length);
  g_free (data);

  return rv;
}

static CK_RV
fill_serial_attribute (CK_ATTRIBUTE *attr,
                       GcrCertificate *cert)
{
  guchar *data;
  gsize length;
  CK_RV rv;

  data = gcr_certificate_get_serial_number (cert, &length);
  rv = fill_data_attribute (attr, data, length);
  g_free (data);

  return rv;
}

static CK_RV
fill_string_attribute (CK_ATTRIBUTE *attr,
                       const gchar *data)
{
  return fill_data_attribute (attr, data, strlen (data));
}

static CK_RV
fill_id_attribute (CK_ATTRIBUTE *attr,
                   GcrCertificate *cert)
{
  gchar *data;
  CK_RV rv;

  data = g_strdup_printf ("%p", cert);
  rv = fill_string_attribute (attr, data);
  g_free (data);

  return rv;
}

static CK_RV
fill_value_attribute (CK_ATTRIBUTE *attr,
                      GcrCertificate *cert)
{
  const guchar *data;
  gsize length;

  data = gcr_certificate_get_der_data (cert, &length);
  return fill_data_attribute (attr, data, length);
}

static CK_RV
fill_ulong_attribute (CK_ATTRIBUTE *attr,
                      CK_ULONG value)
{
  return fill_data_attribute (attr, &value, sizeof (value));
}

static CK_RV
fill_bool_attribute (CK_ATTRIBUTE *attr,
                     CK_BBOOL value)
{
  return fill_data_attribute (attr, &value, sizeof (value));
}

static CK_RV
fill_certificate_attribute (CK_ATTRIBUTE *attr,
                            GcrCertificate *cert)
{
  switch (attr->type)
  {
  case CKA_CLASS:
    return fill_ulong_attribute (attr, CKO_CERTIFICATE);
  case CKA_TOKEN:
    return fill_bool_attribute (attr, CK_TRUE);
  case CKA_PRIVATE:
  case CKA_MODIFIABLE:
  case CKA_TRUSTED:
    return fill_bool_attribute (attr, CK_FALSE);
  case CKA_LABEL:
    return fill_string_attribute (attr, "Certificate");
  case CKA_CERTIFICATE_TYPE:
    return fill_ulong_attribute (attr, CKC_X_509);
  case CKA_CERTIFICATE_CATEGORY:
    return fill_ulong_attribute (attr, 2);
  case CKA_CHECK_VALUE:
    return fill_check_value_attribute (attr, cert);
  case CKA_START_DATE:
  case CKA_END_DATE:
    return fill_data_attribute (attr, "", 0);
  case CKA_SUBJECT:
    return fill_subject_attribute (attr, cert);
  case CKA_ID:
    return fill_id_attribute (attr, cert);
  case CKA_ISSUER:
    return fill_issuer_attribute (attr, cert);
  case CKA_SERIAL_NUMBER:
    return fill_serial_attribute (attr, cert);
  case CKA_VALUE:
    return fill_value_attribute (attr, cert);
  case CKA_URL:
  case CKA_HASH_OF_SUBJECT_PUBLIC_KEY:
  case CKA_HASH_OF_ISSUER_PUBLIC_KEY:
  case CKA_JAVA_MIDP_SECURITY_DOMAIN:
  default:
    return CKR_ATTRIBUTE_TYPE_INVALID;
  }
}

static CK_RV
fill_assertion_attribute (CK_ATTRIBUTE *attr,
                          Assertion *assertion)
{
  CK_RV rv;

  switch (attr->type)
  {
  case CKA_CLASS:
    return fill_ulong_attribute (attr, CKO_X_TRUST_ASSERTION);
  case CKA_TOKEN:
    return fill_bool_attribute (attr, CK_TRUE);
  case CKA_PRIVATE:
  case CKA_MODIFIABLE:
  case CKA_TRUSTED:
    return fill_bool_attribute (attr, CK_FALSE);
  case CKA_LABEL:
    return fill_string_attribute (attr, "Assertion");
  case CKA_X_ASSERTION_TYPE:
    return fill_ulong_attribute (attr, assertion->assertion_type);
  case CKA_X_PURPOSE:
    return fill_string_attribute (attr, assertion->purpose);
  case CKA_X_PEER:
    if (!assertion->peer)
      return CKR_ATTRIBUTE_TYPE_INVALID;
    return fill_string_attribute (attr, assertion->peer);
  case CKA_SERIAL_NUMBER:
  case CKA_ISSUER:
    return fill_certificate_attribute (attr, assertion->cert);
  case CKA_X_CERTIFICATE_VALUE:
    attr->type = CKA_VALUE;
    rv = fill_certificate_attribute (attr, assertion->cert);
    attr->type = CKA_X_CERTIFICATE_VALUE;
    return rv;

  default:
    return CKR_ATTRIBUTE_TYPE_INVALID;
  }
}

CK_RV
mock_C_GetAttributeValue (CK_SESSION_HANDLE session,
                          CK_OBJECT_HANDLE object,
                          CK_ATTRIBUTE_PTR template,
                          CK_ULONG count)
{
  CK_RV rv, ret = CKR_OK;
  GcrCertificate *cert;
  Assertion *assertion;
  Session *sess;
  CK_ULONG i;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  cert = g_hash_table_lookup (the_certificates, GUINT_TO_POINTER (object));
  assertion = g_hash_table_lookup (the_assertions, GUINT_TO_POINTER (object));

  if (cert != NULL) {
      for (i = 0; i < count; i++) {
          rv = fill_certificate_attribute (template + i, cert);
          if (rv != CKR_OK)
            template[i].ulValueLen = (CK_ULONG)-1;
          if (ret != CKR_OK)
            ret = rv;
      }
  } else if (assertion != NULL) {
      for (i = 0; i < count; i++) {
          rv = fill_assertion_attribute (template + i, assertion);
          if (rv != CKR_OK)
            template[i].ulValueLen = (CK_ULONG)-1;
          if (ret != CKR_OK)
            ret = rv;
      }
  } else {
      ret = CKR_OBJECT_HANDLE_INVALID;
  }

  return ret;
}

CK_RV
mock_readonly_C_SetAttributeValue (CK_SESSION_HANDLE session,
                                   CK_OBJECT_HANDLE object,
                                   CK_ATTRIBUTE_PTR template,
                                   CK_ULONG count)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess, CKR_SESSION_HANDLE_INVALID);

  return CKR_TOKEN_WRITE_PROTECTED;
}

static gboolean
match_object_attributes (CK_SESSION_HANDLE session,
                         CK_ULONG object,
                         CK_ATTRIBUTE_PTR template,
                         CK_ULONG count)
{
  CK_ATTRIBUTE_PTR values;
  gboolean mismatch = FALSE;
  CK_RV rv;
  CK_ULONG i;

  values = g_new0 (CK_ATTRIBUTE, count);
  for (i = 0; i < count; i++) {
      values[i].type = template[i].type;
      if (template[i].ulValueLen != 0 &&
          template[i].ulValueLen != (CK_ULONG)-1)
          values[i].pValue = g_malloc (template[i].ulValueLen);
      values[i].ulValueLen = template[i].ulValueLen;
  }

  rv = mock_C_GetAttributeValue (session, object, values, count);

  if (rv == CKR_OK) {
      for (i = 0; i < count; i++) {
          if (gcr_comparable_memcmp (values[i].pValue, values[i].ulValueLen,
                                     template[i].pValue, template[i].ulValueLen) != 0) {
            mismatch = TRUE;
            break;
          }
      }
  }

  for (i = 0; i < count; i++)
      g_free (values[i].pValue);
  g_free (values);

  if (rv != CKR_OK)
    return FALSE;

  return !mismatch;
}

CK_RV
mock_C_FindObjectsInit (CK_SESSION_HANDLE session,
                        CK_ATTRIBUTE_PTR template,
                        CK_ULONG count)
{
  GList *objects = NULL, *l;
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  /* Starting an operation, cancels any previous one */
  if (sess->operation != 0)
    sess->operation = 0;

  sess->operation = OP_FIND;
  g_list_free (sess->matches);
  sess->matches = NULL;

  objects = g_list_concat (objects, g_hash_table_get_keys (the_certificates));
  objects = g_list_concat (objects, g_hash_table_get_keys (the_assertions));

  for (l = objects; l != NULL; l = g_list_next (l)) {
      if (match_object_attributes (session, GPOINTER_TO_UINT (l->data), template, count))
        sess->matches = g_list_prepend (sess->matches, l->data);
  }

  g_list_free (objects);
  return CKR_OK;
}

CK_RV
mock_C_FindObjects (CK_SESSION_HANDLE session,
                    CK_OBJECT_HANDLE_PTR object,
                    CK_ULONG max_object_count,
                    CK_ULONG_PTR object_count)
{
  Session *sess;

  g_return_val_if_fail (object, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail (object_count, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail (max_object_count != 0, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);
  g_return_val_if_fail (sess->operation == OP_FIND, CKR_OPERATION_NOT_INITIALIZED);

  *object_count = 0;
  while (max_object_count > 0 && sess->matches)
    {
      *object = GPOINTER_TO_UINT (sess->matches->data);
      ++object;
      --max_object_count;
      ++(*object_count);
      sess->matches = g_list_remove (sess->matches, sess->matches->data);
    }

  return CKR_OK;
}

CK_RV
mock_C_FindObjectsFinal (CK_SESSION_HANDLE session)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);
  g_return_val_if_fail (sess->operation == OP_FIND, CKR_OPERATION_NOT_INITIALIZED);

  sess->operation = 0;
  g_list_free (sess->matches);
  sess->matches = NULL;

  return CKR_OK;
}

CK_RV
mock_no_mechanisms_C_EncryptInit (CK_SESSION_HANDLE session,
                                  CK_MECHANISM_PTR mechanism,
                                  CK_OBJECT_HANDLE key)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_not_initialized_C_Encrypt (CK_SESSION_HANDLE session,
                                CK_BYTE_PTR data,
                                CK_ULONG data_len,
                                CK_BYTE_PTR encrypted_data,
                                CK_ULONG_PTR encrypted_data_len)
{
  return CKR_OPERATION_NOT_INITIALIZED;
}

CK_RV
mock_unsupported_C_EncryptUpdate (CK_SESSION_HANDLE session,
                                  CK_BYTE_PTR part,
                                  CK_ULONG part_len,
                                  CK_BYTE_PTR encrypted_part,
                                  CK_ULONG_PTR encrypted_part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_EncryptFinal (CK_SESSION_HANDLE session,
                                 CK_BYTE_PTR last_encrypted_part,
                                 CK_ULONG_PTR last_encrypted_part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_no_mechanisms_C_DecryptInit (CK_SESSION_HANDLE session,
                                  CK_MECHANISM_PTR mechanism,
                                  CK_OBJECT_HANDLE key)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_not_initialized_C_Decrypt (CK_SESSION_HANDLE session,
                                CK_BYTE_PTR encrypted_data,
                                CK_ULONG encrypted_data_len,
                                CK_BYTE_PTR data,
                                CK_ULONG_PTR data_len)
{
  return CKR_OPERATION_NOT_INITIALIZED;
}

CK_RV
mock_unsupported_C_DecryptUpdate (CK_SESSION_HANDLE session,
                                  CK_BYTE_PTR encrypted_part,
                                  CK_ULONG encrypted_key_len,
                                  CK_BYTE_PTR part,
                                  CK_ULONG_PTR part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DecryptFinal (CK_SESSION_HANDLE session,
                                 CK_BYTE_PTR last_part,
                                 CK_ULONG_PTR last_part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DigestInit (CK_SESSION_HANDLE session,
                               CK_MECHANISM_PTR mechanism)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_Digest (CK_SESSION_HANDLE session,
                           CK_BYTE_PTR data,
                           CK_ULONG data_len,
                           CK_BYTE_PTR digest,
                           CK_ULONG_PTR digest_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DigestUpdate (CK_SESSION_HANDLE session,
                                 CK_BYTE_PTR part,
                                 CK_ULONG part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DigestKey (CK_SESSION_HANDLE session,
                              CK_OBJECT_HANDLE key)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DigestFinal (CK_SESSION_HANDLE session,
                                CK_BYTE_PTR digest,
                                CK_ULONG_PTR digest_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_no_mechanisms_C_SignInit (CK_SESSION_HANDLE session,
                               CK_MECHANISM_PTR mechanism,
                               CK_OBJECT_HANDLE key)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_not_initialized_C_Sign (CK_SESSION_HANDLE session,
                             CK_BYTE_PTR data,
                             CK_ULONG data_len,
                             CK_BYTE_PTR signature,
                             CK_ULONG_PTR signature_len)
{
  return CKR_OPERATION_NOT_INITIALIZED;
}

CK_RV
mock_unsupported_C_SignUpdate (CK_SESSION_HANDLE session,
                               CK_BYTE_PTR part,
                               CK_ULONG part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SignFinal (CK_SESSION_HANDLE session,
                              CK_BYTE_PTR signature,
                              CK_ULONG_PTR signature_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SignRecoverInit (CK_SESSION_HANDLE session,
                                    CK_MECHANISM_PTR mechanism,
                                    CK_OBJECT_HANDLE key)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SignRecover (CK_SESSION_HANDLE session,
                                CK_BYTE_PTR data,
                                CK_ULONG data_len,
                                CK_BYTE_PTR signature,
                                CK_ULONG_PTR signature_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_no_mechanisms_C_VerifyInit (CK_SESSION_HANDLE session,
                                 CK_MECHANISM_PTR mechanism,
                                 CK_OBJECT_HANDLE key)
{
  Session *sess;

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_not_initialized_C_Verify (CK_SESSION_HANDLE session,
                               CK_BYTE_PTR data,
                               CK_ULONG data_len,
                               CK_BYTE_PTR signature,
                               CK_ULONG signature_len)
{
  return CKR_OPERATION_NOT_INITIALIZED;
}

CK_RV
mock_unsupported_C_VerifyUpdate (CK_SESSION_HANDLE session,
                                 CK_BYTE_PTR part,
                                 CK_ULONG part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_VerifyFinal (CK_SESSION_HANDLE session,
                                CK_BYTE_PTR signature,
                                CK_ULONG signature_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_VerifyRecoverInit (CK_SESSION_HANDLE session,
                                      CK_MECHANISM_PTR mechanism,
                                      CK_OBJECT_HANDLE key)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_VerifyRecover (CK_SESSION_HANDLE session,
                                  CK_BYTE_PTR signature,
                                  CK_ULONG signature_len,
                                  CK_BYTE_PTR data,
                                  CK_ULONG_PTR data_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DigestEncryptUpdate (CK_SESSION_HANDLE session,
                                        CK_BYTE_PTR part,
                                        CK_ULONG part_len,
                                        CK_BYTE_PTR encrypted_part,
                                        CK_ULONG_PTR encrypted_key_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DecryptDigestUpdate (CK_SESSION_HANDLE session,
                                        CK_BYTE_PTR encrypted_part,
                                        CK_ULONG encrypted_key_len,
                                        CK_BYTE_PTR part,
                                        CK_ULONG_PTR part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_SignEncryptUpdate (CK_SESSION_HANDLE session,
                                      CK_BYTE_PTR part,
                                      CK_ULONG part_len,
                                      CK_BYTE_PTR encrypted_part,
                                      CK_ULONG_PTR encrypted_key_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_DecryptVerifyUpdate (CK_SESSION_HANDLE session,
                                        CK_BYTE_PTR encrypted_part,
                                        CK_ULONG encrypted_key_len,
                                        CK_BYTE_PTR part,
                                        CK_ULONG_PTR part_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_GenerateKey (CK_SESSION_HANDLE session,
                                CK_MECHANISM_PTR mechanism,
                                CK_ATTRIBUTE_PTR template,
                                CK_ULONG count,
                                CK_OBJECT_HANDLE_PTR key)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_no_mechanisms_C_GenerateKeyPair (CK_SESSION_HANDLE session,
                                      CK_MECHANISM_PTR mechanism,
                                      CK_ATTRIBUTE_PTR public_key_template,
                                      CK_ULONG public_key_attribute_count,
                                      CK_ATTRIBUTE_PTR private_key_template,
                                      CK_ULONG private_key_attribute_count,
                                      CK_OBJECT_HANDLE_PTR public_key,
                                      CK_OBJECT_HANDLE_PTR private_key)
{
  Session *sess;

  g_return_val_if_fail (mechanism, CKR_MECHANISM_INVALID);
  g_return_val_if_fail (public_key_template, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (public_key_attribute_count, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (private_key_template, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (private_key_attribute_count, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (public_key, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail (private_key, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_no_mechanisms_C_WrapKey (CK_SESSION_HANDLE session,
                              CK_MECHANISM_PTR mechanism,
                              CK_OBJECT_HANDLE wrapping_key,
                              CK_OBJECT_HANDLE key,
                              CK_BYTE_PTR wrapped_key,
                              CK_ULONG_PTR wrapped_key_len)
{
  Session *sess;

  g_return_val_if_fail (mechanism, CKR_MECHANISM_INVALID);
  g_return_val_if_fail (wrapping_key, CKR_OBJECT_HANDLE_INVALID);
  g_return_val_if_fail (key, CKR_OBJECT_HANDLE_INVALID);
  g_return_val_if_fail (wrapped_key_len, CKR_WRAPPED_KEY_LEN_RANGE);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_no_mechanisms_C_UnwrapKey (CK_SESSION_HANDLE session,
                                CK_MECHANISM_PTR mechanism,
                                CK_OBJECT_HANDLE unwrapping_key,
                                CK_BYTE_PTR wrapped_key,
                                CK_ULONG wrapped_key_len,
                                CK_ATTRIBUTE_PTR template,
                                CK_ULONG count,
                                CK_OBJECT_HANDLE_PTR key)
{
  Session *sess;

  g_return_val_if_fail (mechanism, CKR_MECHANISM_INVALID);
  g_return_val_if_fail (unwrapping_key, CKR_WRAPPING_KEY_HANDLE_INVALID);
  g_return_val_if_fail (wrapped_key, CKR_WRAPPED_KEY_INVALID);
  g_return_val_if_fail (wrapped_key_len, CKR_WRAPPED_KEY_LEN_RANGE);
  g_return_val_if_fail (key, CKR_ARGUMENTS_BAD);
  g_return_val_if_fail (template, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (count, CKR_TEMPLATE_INCONSISTENT);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_no_mechanisms_C_DeriveKey (CK_SESSION_HANDLE session,
                                CK_MECHANISM_PTR mechanism,
                                CK_OBJECT_HANDLE base_key,
                                CK_ATTRIBUTE_PTR template,
                                CK_ULONG count,
                                CK_OBJECT_HANDLE_PTR key)
{
  Session *sess;

  g_return_val_if_fail (mechanism, CKR_MECHANISM_INVALID);
  g_return_val_if_fail (count, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (template, CKR_TEMPLATE_INCOMPLETE);
  g_return_val_if_fail (key, CKR_ARGUMENTS_BAD);

  sess = g_hash_table_lookup (the_sessions, GUINT_TO_POINTER (session));
  g_return_val_if_fail (sess != NULL, CKR_SESSION_HANDLE_INVALID);

  return CKR_MECHANISM_INVALID;
}

CK_RV
mock_unsupported_C_SeedRandom (CK_SESSION_HANDLE session,
                               CK_BYTE_PTR pSeed,
                               CK_ULONG seed_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
mock_unsupported_C_GenerateRandom (CK_SESSION_HANDLE session,
                                   CK_BYTE_PTR random_data,
                                   CK_ULONG random_len)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_FUNCTION_LIST mock_default_functions = {
  { 2, 11 },	/* version */
  mock_C_Initialize,
  mock_C_Finalize,
  mock_C_GetInfo,
  mock_C_GetFunctionList,
  mock_C_GetSlotList,
  mock_C_GetSlotInfo,
  mock_C_GetTokenInfo,
  mock_C_GetMechanismList,
  mock_C_GetMechanismInfo,
  mock_unsupported_C_InitToken,
  mock_unsupported_C_InitPIN,
  mock_unsupported_C_SetPIN,
  mock_C_OpenSession,
  mock_C_CloseSession,
  mock_C_CloseAllSessions,
  mock_C_GetSessionInfo,
  mock_unsupported_C_GetOperationState,
  mock_unsupported_C_SetOperationState,
  mock_unsupported_C_Login,
  mock_unsupported_C_Logout,
  mock_readonly_C_CreateObject,
  mock_unsupported_C_CopyObject,
  mock_readonly_C_DestroyObject,
  mock_unsupported_C_GetObjectSize,
  mock_C_GetAttributeValue,
  mock_readonly_C_SetAttributeValue,
  mock_C_FindObjectsInit,
  mock_C_FindObjects,
  mock_C_FindObjectsFinal,
  mock_no_mechanisms_C_EncryptInit,
  mock_not_initialized_C_Encrypt,
  mock_unsupported_C_EncryptUpdate,
  mock_unsupported_C_EncryptFinal,
  mock_no_mechanisms_C_DecryptInit,
  mock_not_initialized_C_Decrypt,
  mock_unsupported_C_DecryptUpdate,
  mock_unsupported_C_DecryptFinal,
  mock_unsupported_C_DigestInit,
  mock_unsupported_C_Digest,
  mock_unsupported_C_DigestUpdate,
  mock_unsupported_C_DigestKey,
  mock_unsupported_C_DigestFinal,
  mock_no_mechanisms_C_SignInit,
  mock_not_initialized_C_Sign,
  mock_unsupported_C_SignUpdate,
  mock_unsupported_C_SignFinal,
  mock_unsupported_C_SignRecoverInit,
  mock_unsupported_C_SignRecover,
  mock_no_mechanisms_C_VerifyInit,
  mock_not_initialized_C_Verify,
  mock_unsupported_C_VerifyUpdate,
  mock_unsupported_C_VerifyFinal,
  mock_unsupported_C_VerifyRecoverInit,
  mock_unsupported_C_VerifyRecover,
  mock_unsupported_C_DigestEncryptUpdate,
  mock_unsupported_C_DecryptDigestUpdate,
  mock_unsupported_C_SignEncryptUpdate,
  mock_unsupported_C_DecryptVerifyUpdate,
  mock_unsupported_C_GenerateKey,
  mock_no_mechanisms_C_GenerateKeyPair,
  mock_no_mechanisms_C_WrapKey,
  mock_no_mechanisms_C_UnwrapKey,
  mock_no_mechanisms_C_DeriveKey,
  mock_unsupported_C_SeedRandom,
  mock_unsupported_C_GenerateRandom,
  mock_C_GetFunctionStatus,
  mock_C_CancelFunction,
  mock_unsupported_C_WaitForSlotEvent
};
