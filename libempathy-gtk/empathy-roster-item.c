#include "config.h"

#include "empathy-roster-item.h"

G_DEFINE_TYPE (EmpathyRosterItem, empathy_roster_item, GTK_TYPE_BOX)

enum
{
  PROP_INDIVIDIUAL = 1,
  N_PROPS
};

/*
enum
{
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _EmpathyRosterItemPriv
{
  FolksIndividual *individual;
};

static void
empathy_roster_item_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterItem *self = EMPATHY_ROSTER_ITEM (object);

  switch (property_id)
    {
      case PROP_INDIVIDIUAL:
        g_value_set_object (value, self->priv->individual);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_item_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyRosterItem *self = EMPATHY_ROSTER_ITEM (object);

  switch (property_id)
    {
      case PROP_INDIVIDIUAL:
        g_assert (self->priv->individual == NULL); /* construct only */
        self->priv->individual = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_roster_item_constructed (GObject *object)
{
  EmpathyRosterItem *self = EMPATHY_ROSTER_ITEM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_item_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (FOLKS_IS_INDIVIDUAL (self->priv->individual));
}

static void
empathy_roster_item_dispose (GObject *object)
{
  EmpathyRosterItem *self = EMPATHY_ROSTER_ITEM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_item_parent_class)->dispose;

  g_clear_object (&self->priv->individual);

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_item_finalize (GObject *object)
{
  //EmpathyRosterItem *self = EMPATHY_ROSTER_ITEM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_item_parent_class)->finalize;

  if (chain_up != NULL)
    chain_up (object);
}

static void
empathy_roster_item_class_init (
    EmpathyRosterItemClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GParamSpec *spec;

  oclass->get_property = empathy_roster_item_get_property;
  oclass->set_property = empathy_roster_item_set_property;
  oclass->constructed = empathy_roster_item_constructed;
  oclass->dispose = empathy_roster_item_dispose;
  oclass->finalize = empathy_roster_item_finalize;

  spec = g_param_spec_object ("individual", "Individual",
      "FolksIndividual",
      FOLKS_TYPE_INDIVIDUAL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_INDIVIDIUAL, spec);

  g_type_class_add_private (klass, sizeof (EmpathyRosterItemPriv));
}

static void
empathy_roster_item_init (EmpathyRosterItem *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_ITEM, EmpathyRosterItemPriv);

  gtk_widget_set_size_request (GTK_WIDGET (self), 300, 64);
}

GtkWidget *
empathy_roster_item_new (FolksIndividual *individual)
{
  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (individual), NULL);

  return g_object_new (EMPATHY_TYPE_ROSTER_ITEM,
      "individual", individual,
      "spacing", 8,
      NULL);
}

FolksIndividual *
empathy_roster_item_get_individual (EmpathyRosterItem *self)
{
  return self->priv->individual;
}
