#include "config.h"

#include "empathy-roster-group.h"

#include <telepathy-glib/telepathy-glib.h>

G_DEFINE_TYPE (EmpathyRosterGroup, empathy_roster_group, GTK_TYPE_EXPANDER)

enum
{
  PROP_NAME = 1,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyRosterGroupPriv
{
  gchar *name;

  /* Widgets associated with this group. EmpathyRosterGroup is not responsible
   * of packing/displaying these widgets. This hash table is a just a set
   * to keep track of them. */
  GHashTable *widgets;
};

static void
empathy_roster_group_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterGroup *self = EMPATHY_ROSTER_GROUP (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_group_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterGroup *self = EMPATHY_ROSTER_GROUP (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_assert (self->priv->name == NULL); /* construct-only */
        self->priv->name = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_group_constructed (GObject *object)
{
  EmpathyRosterGroup *self = EMPATHY_ROSTER_GROUP (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_group_parent_class)->constructed;
  gchar *tmp;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->name != NULL);

  tmp = g_strdup_printf ("<b>%s</b>", self->priv->name);
  gtk_expander_set_label (GTK_EXPANDER (self), tmp);
  g_free (tmp);
}

static void
empathy_roster_group_dispose (GObject *object)
{
  EmpathyRosterGroup *self = EMPATHY_ROSTER_GROUP (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_group_parent_class)->dispose;

  tp_clear_pointer (&self->priv->widgets, g_hash_table_unref);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_group_finalize (GObject *object)
{
  EmpathyRosterGroup *self = EMPATHY_ROSTER_GROUP (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_group_parent_class)->finalize;

  g_free (self->priv->name);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_group_class_init (
    EmpathyRosterGroupClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_roster_group_get_property;
  oclass->set_property = empathy_roster_group_set_property;
  oclass->constructed = empathy_roster_group_constructed;
  oclass->dispose = empathy_roster_group_dispose;
  oclass->finalize = empathy_roster_group_finalize;

  spec = g_param_spec_string ("name", "Name",
      "Group name",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_NAME, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterGroupPriv));
}

static void
empathy_roster_group_init (EmpathyRosterGroup *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_GROUP, EmpathyRosterGroupPriv);

  self->priv->widgets = g_hash_table_new (NULL, NULL);
}

GtkWidget *
empathy_roster_group_new (const gchar *name)
{
  return g_object_new (EMPATHY_TYPE_ROSTER_GROUP,
      "name", name,
      "use-markup", TRUE,
      "expanded", TRUE,
      NULL);
}

const gchar *
empathy_roster_group_get_name (EmpathyRosterGroup *self)
{
  return self->priv->name;
}

guint
empathy_roster_group_add_widget (EmpathyRosterGroup *self,
    GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  g_hash_table_add (self->priv->widgets, widget);

  return empathy_roster_group_get_widgets_count (self);
}

guint
empathy_roster_group_remove_widget (EmpathyRosterGroup *self,
    GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  g_hash_table_remove (self->priv->widgets, widget);

  return empathy_roster_group_get_widgets_count (self);
}

guint
empathy_roster_group_get_widgets_count (EmpathyRosterGroup *self)
{
  return g_hash_table_size (self->priv->widgets);
}

GList *
empathy_roster_group_get_widgets (EmpathyRosterGroup *self)
{
  return g_hash_table_get_keys (self->priv->widgets);
}
