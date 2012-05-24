
#include "config.h"

#include "empathy-roster-view.h"

#include <libempathy-gtk/empathy-roster-item.h>

G_DEFINE_TYPE (EmpathyRosterView, empathy_roster_view, EGG_TYPE_LIST_BOX)

enum
{
  PROP_MANAGER = 1,
  PROP_SHOW_OFFLINE,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyRosterViewPriv
{
  EmpathyIndividualManager *manager;

  /* FolksIndividual (borrowed) -> EmpathyRosterItem (borrowed) */
  GHashTable *items;

  gboolean show_offline;
};

static void
empathy_roster_view_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);

  switch (property_id)
    {
      case PROP_MANAGER:
        g_value_set_object (value, self->priv->manager);
        break;
      case PROP_SHOW_OFFLINE:
        g_value_set_boolean (value, self->priv->show_offline);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_view_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);

  switch (property_id)
    {
      case PROP_MANAGER:
        g_assert (self->priv->manager == NULL); /* construct only */
        self->priv->manager = g_value_dup_object (value);
        break;
      case PROP_SHOW_OFFLINE:
        empathy_roster_view_show_offline (self, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
item_changed_cb (GtkWidget *item,
    GParamSpec *spec,
    EmpathyRosterView *self)
{
  egg_list_box_child_changed (EGG_LIST_BOX (self), item);
}

static void
individual_added (EmpathyRosterView *self,
    FolksIndividual *individual)
{
  GtkWidget *item;

  item = g_hash_table_lookup (self->priv->items, individual);
  if (item != NULL)
    return;

  item = empathy_roster_item_new (individual);

  /* Need to refilter if online is changed */
  g_signal_connect (item, "notify::online",
      G_CALLBACK (item_changed_cb), self);

  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (self), item);

  g_hash_table_insert (self->priv->items, individual, item);
}

static void
individual_removed (EmpathyRosterView *self,
    FolksIndividual *individual)
{
  GtkWidget *item;

  item = g_hash_table_lookup (self->priv->items, individual);
  if (item == NULL)
    return;

  gtk_container_remove (GTK_CONTAINER (self), item);

  g_hash_table_remove (self->priv->items, individual);
}

static void
members_changed_cb (EmpathyIndividualManager *manager,
    const gchar *message,
    GList *added,
    GList *removed,
    TpChannelGroupChangeReason reason,
    EmpathyRosterView *self)
{
  GList *l;

  for (l = added; l != NULL; l = g_list_next (l))
    {
      FolksIndividual *individual = l->data;

      individual_added (self, individual);
    }

  for (l = removed; l != NULL; l = g_list_next (l))
    {
      FolksIndividual *individual = l->data;

      individual_removed (self, individual);
    }
}

static gint
roster_view_sort (EmpathyRosterItem *a,
    EmpathyRosterItem *b,
    EmpathyRosterView *self)
{
  FolksIndividual *ind_a, *ind_b;
  const gchar *alias_a, *alias_b;

  ind_a = empathy_roster_item_get_individual (a);
  ind_b = empathy_roster_item_get_individual (b);

  alias_a = folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (ind_a));
  alias_b = folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (ind_b));

  return g_ascii_strcasecmp (alias_a, alias_b);
}

static void
update_separator (GtkWidget **separator,
    GtkWidget *child,
    GtkWidget *before,
    gpointer user_data)
{
  if (before == NULL)
    {
      /* No separator before the first row */
      g_clear_object (separator);
      return;
    }

  if (*separator != NULL)
    return;

  *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  g_object_ref_sink (*separator);
}

static gboolean
filter_list (GtkWidget *child,
    gpointer user_data)
{
  EmpathyRosterView *self = user_data;
  EmpathyRosterItem *item = EMPATHY_ROSTER_ITEM (child);

  if (self->priv->show_offline)
    return TRUE;

  return empathy_roster_item_is_online (item);
}

static void
empathy_roster_view_constructed (GObject *object)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_view_parent_class)->constructed;
  GList *individuals, *l;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (EMPATHY_IS_INDIVIDUAL_MANAGER (self->priv->manager));

  individuals = empathy_individual_manager_get_members (self->priv->manager);
  for (l = individuals; l != NULL; l = g_list_next (l))
    {
      FolksIndividual *individual = l->data;

      individual_added (self, individual);
    }

  tp_g_signal_connect_object (self->priv->manager, "members-changed",
      G_CALLBACK (members_changed_cb), self, 0);

  g_list_free (individuals);

  egg_list_box_set_sort_func (EGG_LIST_BOX (self),
      (GCompareDataFunc) roster_view_sort, self, NULL);

  egg_list_box_set_separator_funcs (EGG_LIST_BOX (self), update_separator,
      self, NULL);

  egg_list_box_set_filter_func (EGG_LIST_BOX (self), filter_list, self, NULL);
}

static void
empathy_roster_view_dispose (GObject *object)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_view_parent_class)->dispose;

  g_clear_object (&self->priv->manager);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_view_finalize (GObject *object)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_view_parent_class)->finalize;

  g_hash_table_unref (self->priv->items);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_view_class_init (
    EmpathyRosterViewClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_roster_view_get_property;
  oclass->set_property = empathy_roster_view_set_property;
  oclass->constructed = empathy_roster_view_constructed;
  oclass->dispose = empathy_roster_view_dispose;
  oclass->finalize = empathy_roster_view_finalize;

  spec = g_param_spec_object ("manager", "Manager",
      "EmpathyIndividualManager",
      EMPATHY_TYPE_INDIVIDUAL_MANAGER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_MANAGER, spec);

  spec = g_param_spec_boolean ("show-offline", "Show Offline",
      "Show offline contacts",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_SHOW_OFFLINE, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterViewPriv));
}

static void
empathy_roster_view_init (EmpathyRosterView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_VIEW, EmpathyRosterViewPriv);

  self->priv->items = g_hash_table_new (NULL, NULL);
}

GtkWidget *
empathy_roster_view_new (EmpathyIndividualManager *manager)
{
  g_return_val_if_fail (EMPATHY_IS_INDIVIDUAL_MANAGER (manager), NULL);

  return g_object_new (EMPATHY_TYPE_ROSTER_VIEW,
      "manager", manager,
      NULL);
}

EmpathyIndividualManager *
empathy_roster_view_get_manager (EmpathyRosterView *self)
{
  return self->priv->manager;
}

void
empathy_roster_view_show_offline (EmpathyRosterView *self,
    gboolean show)
{
  if (self->priv->show_offline == show)
    return;

  self->priv->show_offline = show;
  egg_list_box_refilter (EGG_LIST_BOX (self));

  g_object_notify (G_OBJECT (self), "show-offline");
}
