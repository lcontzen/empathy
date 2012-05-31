#include "config.h"

#include "empathy-roster-view.h"

#include <glib/gi18n-lib.h>

#include <libempathy-gtk/empathy-roster-contact.h>
#include <libempathy-gtk/empathy-roster-group.h>
#include <libempathy-gtk/empathy-ui-utils.h>

G_DEFINE_TYPE (EmpathyRosterView, empathy_roster_view, EGG_TYPE_LIST_BOX)

enum
{
  PROP_MANAGER = 1,
  PROP_SHOW_OFFLINE,
  PROP_SHOW_GROUPS,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

#define NO_GROUP "X-no-group"
#define UNGROUPPED _("Ungroupped")
#define TOP_GROUP _("Most Used")

struct _EmpathyRosterViewPriv
{
  EmpathyIndividualManager *manager;

  /* FolksIndividual (borrowed) -> GHashTable (
   * (gchar * group_name) -> EmpathyRosterContact (borrowed))
   *
   * When not using groups, this hash just have one element mapped
   * from the special NO_GROUP key. We could use it as a set but
   * I prefer to stay coherent in the way this hash is managed.
   */
  GHashTable *roster_contacts;
  /* (gchar *group_name) -> EmpathyRosterGroup (borrowed) */
  GHashTable *roster_groups;

  gboolean show_offline;
  gboolean show_groups;

  EmpathyLiveSearch *search;
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
      case PROP_SHOW_GROUPS:
        g_value_set_boolean (value, self->priv->show_groups);
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
      case PROP_SHOW_GROUPS:
        empathy_roster_view_show_groups (self, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
roster_contact_changed_cb (GtkWidget *child,
    GParamSpec *spec,
    EmpathyRosterView *self)
{
  egg_list_box_child_changed (EGG_LIST_BOX (self), child);
}

static GtkWidget *
add_roster_contact (EmpathyRosterView *self,
    FolksIndividual *individual,
    const gchar *group)
{
  GtkWidget *contact;

  contact = empathy_roster_contact_new (individual, group);

  /* Need to refilter if online is changed */
  g_signal_connect (contact, "notify::online",
      G_CALLBACK (roster_contact_changed_cb), self);

  /* Need to resort if alias is changed */
  g_signal_connect (contact, "notify::alias",
      G_CALLBACK (roster_contact_changed_cb), self);

  gtk_widget_show (contact);
  gtk_container_add (GTK_CONTAINER (self), contact);

  return contact;
}

static void
group_expanded_cb (EmpathyRosterGroup *group,
    GParamSpec *spec,
    EmpathyRosterView *self)
{
  GList *widgets, *l;

  widgets = empathy_roster_group_get_widgets (group);
  for (l = widgets; l != NULL; l = g_list_next (l))
    {
      egg_list_box_child_changed (EGG_LIST_BOX (self), l->data);
    }

  g_list_free (widgets);
}

static EmpathyRosterGroup *
lookup_roster_group (EmpathyRosterView *self,
    const gchar *group)
{
  return g_hash_table_lookup (self->priv->roster_groups, group);
}

static void
ensure_roster_group (EmpathyRosterView *self,
    const gchar *group)
{
  GtkWidget *roster_group;

  roster_group = (GtkWidget *) lookup_roster_group (self, group);
  if (roster_group != NULL)
    return;

  roster_group = empathy_roster_group_new (group);

  g_signal_connect (roster_group, "notify::expanded",
      G_CALLBACK (group_expanded_cb), self);

  gtk_widget_show (roster_group);
  gtk_container_add (GTK_CONTAINER (self), roster_group);

  g_hash_table_insert (self->priv->roster_groups, g_strdup (group),
      roster_group);
}

static void
add_to_group (EmpathyRosterView *self,
    FolksIndividual *individual,
    const gchar *group)
{
  GtkWidget *contact;
  GHashTable *contacts;

  contacts = g_hash_table_lookup (self->priv->roster_contacts, individual);
  if (contacts == NULL)
    return;

  if (tp_strdiff (group, NO_GROUP))
    ensure_roster_group (self, group);

  contact = add_roster_contact (self, individual, group);
  g_hash_table_insert (contacts, g_strdup (group), contact);
}

static void
individual_added (EmpathyRosterView *self,
    FolksIndividual *individual)
{
  GHashTable *contacts;

  contacts = g_hash_table_lookup (self->priv->roster_contacts, individual);
  if (contacts != NULL)
    return;

  contacts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_hash_table_insert (self->priv->roster_contacts, individual, contacts);

  if (!self->priv->show_groups)
    {
      add_to_group (self, individual, NO_GROUP);
    }
  else
    {
      GeeSet *groups;

      groups = folks_group_details_get_groups (
          FOLKS_GROUP_DETAILS (individual));

      if (gee_collection_get_size (GEE_COLLECTION (groups)) > 0)
        {
          GeeIterator *iter = gee_iterable_iterator (GEE_ITERABLE (groups));

          while (iter != NULL && gee_iterator_next (iter))
            {
              gchar *group = gee_iterator_get (iter);

              add_to_group (self, individual, group);

              g_free (group);
            }

          g_clear_object (&iter);
        }
      else
        {
          /* No group, adds to Ungroupped */
          add_to_group (self, individual, UNGROUPPED);
        }
    }
}

static void
update_group_widgets_count (EmpathyRosterView *self,
    EmpathyRosterGroup *group,
    EmpathyRosterContact *contact,
    gboolean displayed)
{
  if (displayed)
    {
      if (empathy_roster_group_add_widget (group, GTK_WIDGET (contact)) == 1)
        {
          egg_list_box_child_changed (EGG_LIST_BOX (self),
              GTK_WIDGET (group));
        }
    }
  else
    {
      if (empathy_roster_group_remove_widget (group, GTK_WIDGET (contact)) == 0)
        {
          egg_list_box_child_changed (EGG_LIST_BOX (self),
              GTK_WIDGET (group));
        }
    }
}

static void
individual_removed (EmpathyRosterView *self,
    FolksIndividual *individual)
{
  GHashTable *contacts;
  GHashTableIter iter;
  gpointer key, value;

  contacts = g_hash_table_lookup (self->priv->roster_contacts, individual);
  if (contacts == NULL)
    return;

  g_hash_table_iter_init (&iter, contacts);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *group_name = key;
      GtkWidget *contact = value;
      EmpathyRosterGroup *group;

      group = lookup_roster_group (self, group_name);
      if (group != NULL)
        {
          update_group_widgets_count (self, group,
              EMPATHY_ROSTER_CONTACT (contact), FALSE);
        }

      gtk_container_remove (GTK_CONTAINER (self), contact);
    }

  g_hash_table_remove (self->priv->roster_contacts, individual);
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
compare_roster_contacts_by_alias (EmpathyRosterContact *a,
    EmpathyRosterContact *b)
{
  FolksIndividual *ind_a, *ind_b;
  const gchar *alias_a, *alias_b;

  ind_a = empathy_roster_contact_get_individual (a);
  ind_b = empathy_roster_contact_get_individual (b);

  alias_a = folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (ind_a));
  alias_b = folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (ind_b));

  return g_ascii_strcasecmp (alias_a, alias_b);
}

static gint
compare_individual_top_position (EmpathyRosterView *self,
    EmpathyRosterContact *a,
    EmpathyRosterContact *b)
{
  FolksIndividual *ind_a, *ind_b;
  GList *tops;
  gint index_a, index_b;

  ind_a = empathy_roster_contact_get_individual (a);
  ind_b = empathy_roster_contact_get_individual (b);

  tops = empathy_individual_manager_get_top_individuals (self->priv->manager);

  index_a = g_list_index (tops, ind_a);
  index_b = g_list_index (tops, ind_b);

  if (index_a == index_b)
    return 0;

  if (index_a == -1)
    return 1;

  if (index_b == -1)
    return -1;

  return index_a - index_b;
}

static gint
compare_roster_contacts_no_group (EmpathyRosterView *self,
    EmpathyRosterContact *a,
    EmpathyRosterContact *b)
{
  gint top;

  top = compare_individual_top_position (self, a, b);
  if (top != 0)
    return top;

  return compare_roster_contacts_by_alias (a, b);
}

static gint
compare_group_names (const gchar *group_a,
    const gchar *group_b)
{
  if (!tp_strdiff (group_a, TOP_GROUP))
    return -1;

  if (!tp_strdiff (group_b, TOP_GROUP))
    return 1;

  return g_ascii_strcasecmp (group_a, group_b);
}

static gint
compare_roster_contacts_with_groups (EmpathyRosterView *self,
    EmpathyRosterContact *a,
    EmpathyRosterContact *b)
{
  const gchar *group_a, *group_b;

  group_a = empathy_roster_contact_get_group (a);
  group_b = empathy_roster_contact_get_group (b);

  if (!tp_strdiff (group_a, group_b))
    /* Same group, compare the contacts */
    return compare_roster_contacts_by_alias (a, b);

  /* Sort by group */
  return compare_group_names (group_a, group_b);
}

static gint
compare_roster_contacts (EmpathyRosterView *self,
    EmpathyRosterContact *a,
    EmpathyRosterContact *b)
{
  if (!self->priv->show_groups)
    return compare_roster_contacts_no_group (self, a, b);
  else
    return compare_roster_contacts_with_groups (self, a, b);
}

static gint
compare_roster_groups (EmpathyRosterGroup *a,
    EmpathyRosterGroup *b)
{
  const gchar *name_a, *name_b;

  name_a = empathy_roster_group_get_name (a);
  name_b = empathy_roster_group_get_name (b);

  return compare_group_names (name_a, name_b);
}

static gint
compare_contact_group (EmpathyRosterContact *contact,
    EmpathyRosterGroup *group)
{
  const char *contact_group, *group_name;

  contact_group = empathy_roster_contact_get_group (contact);
  group_name = empathy_roster_group_get_name (group);

  if (!tp_strdiff (contact_group, group_name))
    /* @contact is in @group, @group has to be displayed first */
    return 1;

  /* @contact is in a different group, sort by group name */
  return compare_group_names (contact_group, group_name);
}

static gint
roster_view_sort (gconstpointer a,
    gconstpointer b,
    gpointer user_data)
{
  EmpathyRosterView *self = user_data;

  if (EMPATHY_IS_ROSTER_CONTACT (a) && EMPATHY_IS_ROSTER_CONTACT (b))
    return compare_roster_contacts (self, EMPATHY_ROSTER_CONTACT (a),
        EMPATHY_ROSTER_CONTACT (b));
  else if (EMPATHY_IS_ROSTER_GROUP (a) && EMPATHY_IS_ROSTER_GROUP (b))
    return compare_roster_groups (EMPATHY_ROSTER_GROUP (a),
        EMPATHY_ROSTER_GROUP (b));
  else if (EMPATHY_IS_ROSTER_CONTACT (a) && EMPATHY_IS_ROSTER_GROUP (b))
    return compare_contact_group (EMPATHY_ROSTER_CONTACT (a),
        EMPATHY_ROSTER_GROUP (b));
  else if (EMPATHY_IS_ROSTER_GROUP (a) && EMPATHY_IS_ROSTER_CONTACT (b))
    return -1 * compare_contact_group (EMPATHY_ROSTER_CONTACT (b),
        EMPATHY_ROSTER_GROUP (a));

  g_return_val_if_reached (0);
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
is_searching (EmpathyRosterView *self)
{
  if (self->priv->search == NULL)
    return FALSE;

  return gtk_widget_get_visible (GTK_WIDGET (self->priv->search));
}

static gboolean
filter_contact (EmpathyRosterView *self,
    EmpathyRosterContact *contact)
{
  gboolean displayed;

  if (is_searching (self))
    {
      FolksIndividual *individual;

      individual = empathy_roster_contact_get_individual (contact);

      displayed = empathy_individual_match_string (individual,
          empathy_live_search_get_text (self->priv->search),
          empathy_live_search_get_words (self->priv->search));
    }
  else
    {
      if (self->priv->show_offline)
        {
          displayed = TRUE;
        }
      else
        {
          displayed = empathy_roster_contact_is_online (contact);
        }
    }

  if (self->priv->show_groups)
    {
      const gchar *group_name;
      EmpathyRosterGroup *group;

      group_name = empathy_roster_contact_get_group (contact);
      group = lookup_roster_group (self, group_name);

      if (group != NULL)
        {
          update_group_widgets_count (self, group, contact, displayed);

          /* When searching, always display even if the group is closed */
          if (!is_searching (self) &&
              !gtk_expander_get_expanded (GTK_EXPANDER (group)))
            return FALSE;
        }
    }

  return displayed;
}

static gboolean
filter_group (EmpathyRosterView *self,
    EmpathyRosterGroup *group)
{
  return empathy_roster_group_get_widgets_count (group);
}

static gboolean
filter_list (GtkWidget *child,
    gpointer user_data)
{
  EmpathyRosterView *self = user_data;

  if (EMPATHY_IS_ROSTER_CONTACT (child))
    return filter_contact (self, EMPATHY_ROSTER_CONTACT (child));

  else if (EMPATHY_IS_ROSTER_GROUP (child))
    return filter_group (self, EMPATHY_ROSTER_GROUP (child));

  g_return_val_if_reached (FALSE);
}

/* @list: GList of EmpathyRosterContact
 *
 * Returns: %TRUE if @list contains an EmpathyRosterContact associated with
 * @individual */
static gboolean
individual_in_list (FolksIndividual *individual,
    GList *list)
{
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      EmpathyRosterContact *contact = l->data;

      if (empathy_roster_contact_get_individual (contact) == individual)
        return TRUE;
    }

  return FALSE;
}

static void
populate_view (EmpathyRosterView *self)
{
  GList *individuals, *l;

  individuals = empathy_individual_manager_get_members (self->priv->manager);
  for (l = individuals; l != NULL; l = g_list_next (l))
    {
      FolksIndividual *individual = l->data;

      individual_added (self, individual);
    }

  g_list_free (individuals);
}

static void
remove_from_group (EmpathyRosterView *self,
    FolksIndividual *individual,
    const gchar *group)
{
  GHashTable *contacts;
  GtkWidget *contact;
  EmpathyRosterGroup *roster_group;

  contacts = g_hash_table_lookup (self->priv->roster_contacts, individual);
  if (contacts == NULL)
    return;

  contact = g_hash_table_lookup (contacts, group);
  if (contact == NULL)
    return;

  g_hash_table_remove (contacts, group);

  if (g_hash_table_size (contacts) == 0)
    {
      add_to_group (self, individual, UNGROUPPED);
    }

  roster_group = lookup_roster_group (self, group);

  if (roster_group != NULL)
    {
      update_group_widgets_count (self, roster_group,
          EMPATHY_ROSTER_CONTACT (contact), FALSE);
    }

  gtk_container_remove (GTK_CONTAINER (self), contact);
}

static void
update_top_contacts (EmpathyRosterView *self)
{
  GList *tops, *l;
  GList *to_add = NULL, *to_remove = NULL;
  EmpathyRosterGroup *group;

  if (!self->priv->show_groups)
    {
      egg_list_box_resort (EGG_LIST_BOX (self));
      return;
    }

  tops = empathy_individual_manager_get_top_individuals (self->priv->manager);

  group = g_hash_table_lookup (self->priv->roster_groups, TOP_GROUP);
  if (group == NULL)
    {
      to_add = g_list_copy (tops);
    }
  else
    {
      GList *contacts;

      contacts = empathy_roster_group_get_widgets (group);

      /* Check which EmpathyRosterContact have to be removed */
      for (l = contacts; l != NULL; l = g_list_next (l))
        {
          EmpathyRosterContact *contact = l->data;
          FolksIndividual *individual;

          individual = empathy_roster_contact_get_individual (contact);

          if (g_list_find (tops, individual) == NULL)
            to_remove = g_list_prepend (to_remove, individual);
        }

      /* Check which EmpathyRosterContact have to be added */
      for (l = tops; l != NULL; l = g_list_next (l))
        {
          FolksIndividual *individual = l->data;

          if (!individual_in_list (individual, contacts))
            to_add = g_list_prepend (to_add, individual);
        }
    }

  for (l = to_add; l != NULL; l = g_list_next (l))
    add_to_group (self, l->data, TOP_GROUP);

  for (l = to_remove; l != NULL; l = g_list_next (l))
    remove_from_group (self, l->data, TOP_GROUP);

  g_list_free (to_add);
  g_list_free (to_remove);
}

static void
groups_changed_cb (EmpathyIndividualManager *manager,
    FolksIndividual *individual,
    gchar *group,
    gboolean is_member,
    EmpathyRosterView *self)
{
  if (!self->priv->show_groups)
    return;

  if (is_member)
    {
      add_to_group (self, individual, group);
    }
  else
    {
      remove_from_group (self, individual, group);
    }
}

static void
top_individuals_changed_cb (EmpathyIndividualManager *manager,
    GParamSpec *spec,
    EmpathyRosterView *self)
{
  update_top_contacts (self);
}

static void
empathy_roster_view_constructed (GObject *object)
{
  EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_view_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (EMPATHY_IS_INDIVIDUAL_MANAGER (self->priv->manager));

  populate_view (self);

  tp_g_signal_connect_object (self->priv->manager, "members-changed",
      G_CALLBACK (members_changed_cb), self, 0);
  tp_g_signal_connect_object (self->priv->manager, "groups-changed",
      G_CALLBACK (groups_changed_cb), self, 0);
  tp_g_signal_connect_object (self->priv->manager, "notify::top-individuals",
      G_CALLBACK (top_individuals_changed_cb), self, 0);

  egg_list_box_set_sort_func (EGG_LIST_BOX (self),
      roster_view_sort, self, NULL);

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

  empathy_roster_view_set_live_search (self, NULL);
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

  g_hash_table_unref (self->priv->roster_contacts);
  g_hash_table_unref (self->priv->roster_groups);

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

  spec = g_param_spec_boolean ("show-groups", "Show Groups",
      "Show groups",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_SHOW_GROUPS, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterViewPriv));
}

static void
empathy_roster_view_init (EmpathyRosterView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_VIEW, EmpathyRosterViewPriv);

  self->priv->roster_contacts = g_hash_table_new_full (NULL, NULL,
      NULL, (GDestroyNotify) g_hash_table_unref);
  self->priv->roster_groups = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
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

static void
clear_view (EmpathyRosterView *self)
{
  gtk_container_foreach (GTK_CONTAINER (self),
      (GtkCallback) gtk_widget_destroy, NULL);

  g_hash_table_remove_all (self->priv->roster_contacts);
}

void
empathy_roster_view_show_groups (EmpathyRosterView *self,
    gboolean show)
{
  if (self->priv->show_groups == show)
    return;

  self->priv->show_groups = show;

  /* TODO: block sort/filter? */
  clear_view (self);
  populate_view (self);

  g_object_notify (G_OBJECT (self), "show-groups");
}

static void
select_first_contact (EmpathyRosterView *self)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self));
  for (l = children; l != NULL; l = g_list_next (l))
    {
      GtkWidget *child = l->data;

      if (!gtk_widget_get_child_visible (child))
        continue;

      if (!EMPATHY_IS_ROSTER_CONTACT (child))
        continue;

      egg_list_box_select_child (EGG_LIST_BOX (self), child);
      break;
    }

  g_list_free (children);
}

static void
search_text_notify_cb (EmpathyLiveSearch *search,
    GParamSpec *pspec,
    EmpathyRosterView *self)
{
  egg_list_box_refilter (EGG_LIST_BOX (self));

  select_first_contact (self);
}

static void
search_activate_cb (GtkWidget *search,
  EmpathyRosterView *self)
{
  /* TODO */
}

void
empathy_roster_view_set_live_search (EmpathyRosterView *self,
    EmpathyLiveSearch *search)
{
  if (self->priv->search != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->priv->search,
          search_text_notify_cb, self);
      g_signal_handlers_disconnect_by_func (self->priv->search,
          search_activate_cb, self);

      g_clear_object (&self->priv->search);
    }

  if (search == NULL)
    return;

  self->priv->search = g_object_ref (search);

  g_signal_connect (self->priv->search, "notify::text",
      G_CALLBACK (search_text_notify_cb), self);
  g_signal_connect (self->priv->search, "activate",
      G_CALLBACK (search_activate_cb), self);
}
