#ifndef __EMPATHY_ROSTER_ITEM_H__
#define __EMPATHY_ROSTER_ITEM_H__

#include <gtk/gtk.h>
#include <folks/folks.h>

G_BEGIN_DECLS

typedef struct _EmpathyRosterItem EmpathyRosterItem;
typedef struct _EmpathyRosterItemClass EmpathyRosterItemClass;
typedef struct _EmpathyRosterItemPriv EmpathyRosterItemPriv;

struct _EmpathyRosterItemClass
{
  /*<private>*/
  GtkAlignmentClass parent_class;
};

struct _EmpathyRosterItem
{
  /*<private>*/
  GtkAlignment parent;
  EmpathyRosterItemPriv *priv;
};

GType empathy_roster_item_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ROSTER_ITEM \
  (empathy_roster_item_get_type ())
#define EMPATHY_ROSTER_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ROSTER_ITEM, \
    EmpathyRosterItem))
#define EMPATHY_ROSTER_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ROSTER_ITEM, \
    EmpathyRosterItemClass))
#define EMPATHY_IS_ROSTER_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ROSTER_ITEM))
#define EMPATHY_IS_ROSTER_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ROSTER_ITEM))
#define EMPATHY_ROSTER_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ROSTER_ITEM, \
    EmpathyRosterItemClass))

GtkWidget * empathy_roster_item_new (FolksIndividual *individual);

FolksIndividual * empathy_roster_item_get_individual (EmpathyRosterItem *self);

gboolean empathy_roster_item_is_online (EmpathyRosterItem *self);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ROSTER_ITEM_H__*/
