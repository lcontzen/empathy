
#ifndef __EMPATHY_ROSTER_VIEW_H__
#define __EMPATHY_ROSTER_VIEW_H__

#include <libempathy-gtk/egg-list-box/egg-list-box.h>
#include <libempathy-gtk/empathy-live-search.h>

#include <libroster/empathy-roster-model.h>

G_BEGIN_DECLS

typedef struct _EmpathyRosterView EmpathyRosterView;
typedef struct _EmpathyRosterViewClass EmpathyRosterViewClass;
typedef struct _EmpathyRosterViewPriv EmpathyRosterViewPriv;

struct _EmpathyRosterViewClass
{
  /*<private>*/
  EggListBoxClass parent_class;
};

struct _EmpathyRosterView
{
  /*<private>*/
  EggListBox parent;
  EmpathyRosterViewPriv *priv;
};

GType empathy_roster_view_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ROSTER_VIEW \
  (empathy_roster_view_get_type ())
#define EMPATHY_ROSTER_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ROSTER_VIEW, \
    EmpathyRosterView))
#define EMPATHY_ROSTER_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ROSTER_VIEW, \
    EmpathyRosterViewClass))
#define EMPATHY_IS_ROSTER_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ROSTER_VIEW))
#define EMPATHY_IS_ROSTER_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ROSTER_VIEW))
#define EMPATHY_ROSTER_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ROSTER_VIEW, \
    EmpathyRosterViewClass))

GtkWidget * empathy_roster_view_new (EmpathyRosterModel *model);

void empathy_roster_view_show_offline (EmpathyRosterView *self,
    gboolean show);

void empathy_roster_view_show_groups (EmpathyRosterView *self,
    gboolean show);

void empathy_roster_view_set_live_search (EmpathyRosterView *self,
    EmpathyLiveSearch *search);

gboolean empathy_roster_view_is_empty (EmpathyRosterView *self);

gboolean empathy_roster_view_is_searching (EmpathyRosterView *self);

guint empathy_roster_view_add_event (EmpathyRosterView *self,
    FolksIndividual *individual,
    const gchar *icon,
    gpointer user_data);

void empathy_roster_view_remove_event (EmpathyRosterView *self,
    guint event_id);

FolksIndividual * empathy_roster_view_get_individual_at_y (
    EmpathyRosterView *self,
    gint y,
    GtkWidget **out_child);

const gchar * empathy_roster_view_get_group_at_y (
    EmpathyRosterView *self,
    gint y);

FolksIndividual * empathy_roster_view_get_selected_individual (EmpathyRosterView *self);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ROSTER_VIEW_H__*/
