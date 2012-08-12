#include <config.h>

#include <libroster/empathy-roster-model.h>
#include <libempathy-gtk/empathy-roster-model-manager.h>

#include <libroster/empathy-roster-view.h>
#include <libempathy-gtk/empathy-ui-utils.h>

static void
individual_activated_cb (EmpathyRosterView *self,
    FolksIndividual *individual,
    gpointer user_data)
{
  g_assert (FOLKS_IS_INDIVIDUAL (individual));

  g_print ("'%s' activated\n",
      folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (individual)));
}

static void
popup_individual_menu_cb (EmpathyRosterView *self,
    FolksIndividual *individual,
    guint button,
    guint time,
    gpointer user_data)
{
  GtkWidget *menu, *item;

  g_print ("'%s' popup menu\n",
      folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (individual)));

  menu = gtk_menu_new ();

  g_signal_connect (menu, "deactivate",
      G_CALLBACK (gtk_widget_destroy), NULL);

  item = gtk_menu_item_new_with_label (folks_alias_details_get_alias (
          FOLKS_ALIAS_DETAILS (individual)));
  gtk_widget_show (item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (self), NULL);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
}

static gboolean
individual_tooltip_cb (EmpathyRosterView *view,
    FolksIndividual *individual,
    gboolean keyboard_mode,
    GtkTooltip *tooltip,
    gpointer user_data)
{
  gtk_tooltip_set_text (tooltip,
      folks_alias_details_get_alias (FOLKS_ALIAS_DETAILS (individual)));

  return TRUE;
}

static void
empty_cb (EmpathyRosterView *view,
    GParamSpec *spec,
    gpointer user_data)
{
  if (empathy_roster_view_is_empty (view))
    g_print ("view is now empty\n");
  else
    g_print ("view is no longer empty\n");
}

static GtkWidget *
create_view_box (EmpathyRosterModel *model,
    gboolean show_offline,
    gboolean show_groups)
{
  GtkWidget *view, *scrolled, *box, *search;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  view = empathy_roster_view_new (model);

  g_signal_connect (view, "individual-activated",
      G_CALLBACK (individual_activated_cb), NULL);
  g_signal_connect (view, "popup-individual-menu",
      G_CALLBACK (popup_individual_menu_cb), NULL);
  g_signal_connect (view, "notify::empty",
      G_CALLBACK (empty_cb), NULL);
  g_signal_connect (view, "individual-tooltip",
      G_CALLBACK (individual_tooltip_cb), NULL);

  gtk_widget_set_has_tooltip (view, TRUE);

  empathy_roster_view_show_offline (EMPATHY_ROSTER_VIEW (view), show_offline);
  empathy_roster_view_show_groups (EMPATHY_ROSTER_VIEW (view), show_groups);

  search = empathy_roster_live_search_new (view);
  empathy_roster_view_set_roster_live_search (EMPATHY_ROSTER_VIEW (view),
      EMPATHY_ROSTER_LIVE_SEARCH (search));

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  egg_list_box_add_to_scrolled (EGG_LIST_BOX (view),
      GTK_SCROLLED_WINDOW (scrolled));

  gtk_box_pack_start (GTK_BOX (box), search, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scrolled, TRUE, TRUE, 0);

  return box;
}

int
main (int argc,
    char **argv)
{
  GtkWidget *window, *view1, *view2, *box_horiz;
  EmpathyIndividualManager *mgr;
  EmpathyRosterModel *model;

  gtk_init (&argc, &argv);
  empathy_gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  empathy_set_css_provider (window);

  box_horiz = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

  mgr = empathy_individual_manager_dup_singleton ();

  model = EMPATHY_ROSTER_MODEL (empathy_roster_model_manager_new (mgr));

  view1 = create_view_box (model, TRUE, TRUE);
  view2 = create_view_box (model, FALSE, FALSE);

  g_object_unref (model);
  g_object_unref (mgr);

  gtk_box_pack_start (GTK_BOX (box_horiz), view1, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box_horiz), view2, FALSE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), box_horiz);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);
  gtk_widget_show_all (window);

  g_signal_connect_swapped (window, "destroy",
      G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
