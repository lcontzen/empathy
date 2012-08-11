#ifndef __EMPATHY_ROSTER_GROUP_H__
#define __EMPATHY_ROSTER_GROUP_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EmpathyRosterGroup EmpathyRosterGroup;
typedef struct _EmpathyRosterGroupClass EmpathyRosterGroupClass;
typedef struct _EmpathyRosterGroupPriv EmpathyRosterGroupPriv;

struct _EmpathyRosterGroupClass
{
  /*<private>*/
  GtkExpanderClass parent_class;
};

struct _EmpathyRosterGroup
{
  /*<private>*/
  GtkExpander parent;
  EmpathyRosterGroupPriv *priv;
};

GType empathy_roster_group_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ROSTER_GROUP \
  (empathy_roster_group_get_type ())
#define EMPATHY_ROSTER_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ROSTER_GROUP, \
    EmpathyRosterGroup))
#define EMPATHY_ROSTER_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ROSTER_GROUP, \
    EmpathyRosterGroupClass))
#define EMPATHY_IS_ROSTER_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ROSTER_GROUP))
#define EMPATHY_IS_ROSTER_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ROSTER_GROUP))
#define EMPATHY_ROSTER_GROUP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ROSTER_GROUP, \
    EmpathyRosterGroupClass))

GtkWidget * empathy_roster_group_new (const gchar *name,
    const gchar *icon);

const gchar * empathy_roster_group_get_name (EmpathyRosterGroup *self);

guint empathy_roster_group_add_widget (EmpathyRosterGroup *self,
    GtkWidget *widget);
guint empathy_roster_group_remove_widget (EmpathyRosterGroup *self,
    GtkWidget *widget);
guint empathy_roster_group_get_widgets_count (EmpathyRosterGroup *self);
GList * empathy_roster_group_get_widgets (EmpathyRosterGroup *self);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ROSTER_GROUP_H__*/
