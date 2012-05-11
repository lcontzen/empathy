#include <config.h>

#include <libempathy-gtk/empathy-roster-view.h>
#include <libempathy-gtk/empathy-ui-utils.h>

int
main (int argc,
    char **argv)
{
  GtkWidget *window, *view, *scrolled;
  EmpathyIndividualManager *mgr;

  gtk_init (&argc, &argv);
  empathy_gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  empathy_set_css_provider (window);

  mgr = empathy_individual_manager_dup_singleton ();

  view = empathy_roster_view_new (mgr);

  g_object_unref (mgr);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  egg_list_box_add_to_scrolled (EGG_LIST_BOX (view),
      GTK_SCROLLED_WINDOW (scrolled));
  gtk_container_add (GTK_CONTAINER (window), scrolled);

  gtk_window_set_default_size (GTK_WINDOW (window), 300, 600);
  gtk_widget_show_all (window);

  g_signal_connect_swapped (window, "destroy",
      G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
