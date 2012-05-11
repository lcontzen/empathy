
#include "config.h"

#include "empathy-roster-view.h"

G_DEFINE_TYPE (EmpathyRosterView, empathy_roster_view, EGG_TYPE_LIST_BOX)

enum
{
  PROP_MANAGER = 1,
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
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
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
  //EmpathyRosterView *self = EMPATHY_ROSTER_VIEW (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) empathy_roster_view_parent_class)->finalize;

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

  g_type_class_add_private (klass, sizeof (EmpathyRosterViewPriv));
}

static void
empathy_roster_view_init (EmpathyRosterView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_ROSTER_VIEW, EmpathyRosterViewPriv);
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
