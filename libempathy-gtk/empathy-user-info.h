/*
 * empathy-user-info.h - Header for EmpathyUserInfo
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

#ifndef __EMPATHY_USER_INFO_H__
#define __EMPATHY_USER_INFO_H__

#include <gtk/gtk.h>

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_USER_INFO \
    (empathy_user_info_get_type ())
#define EMPATHY_USER_INFO(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_USER_INFO, \
        EmpathyUserInfo))
#define EMPATHY_USER_INFO_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), EMPATHY_TYPE_USER_INFO, \
        EmpathyUserInfoClass))
#define EMPATHY_IS_USER_INFO(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_USER_INFO))
#define EMPATHY_IS_USER_INFO_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_USER_INFO))
#define EMPATHY_USER_INFO_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_USER_INFO, \
        EmpathyUserInfoClass))

typedef struct _EmpathyUserInfo EmpathyUserInfo;
typedef struct _EmpathyUserInfoClass EmpathyUserInfoClass;
typedef struct _EmpathyUserInfoPrivate EmpathyUserInfoPrivate;

struct _EmpathyUserInfo {
  GtkGrid parent;

  EmpathyUserInfoPrivate *priv;
};

struct _EmpathyUserInfoClass {
  GtkGridClass parent_class;
};

GType empathy_user_info_get_type (void) G_GNUC_CONST;

GtkWidget *empathy_user_info_new (TpAccount *account);

void empathy_user_info_discard (EmpathyUserInfo *self);

void empathy_user_info_apply_async (EmpathyUserInfo *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean empathy_user_info_apply_finish (EmpathyUserInfo *self,
    GAsyncResult *result,
    GError **error);


G_END_DECLS

#endif /* __EMPATHY_USER_INFO_H__ */

