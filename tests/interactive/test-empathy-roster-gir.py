from gi.repository import EmpathyRoster
from gi.repository import Folks
from gi.repository import Gtk

class TestEmpathyRosterGIR(Gtk.Window):

    def __init__(self, ):
        Gtk.Window.__init__(self, title="Test EmpathyRoster GIR")

        self.box = Gtk.Box(spacing=8)
        self.box.set_orientation (Gtk.Orientation.VERTICAL)

        self.aggregator = Folks.IndividualAggregator.new ()
        self.model = EmpathyRoster.ModelAggregator.new_with_aggregator (
            self.aggregator, None, None)

        self.view = EmpathyRoster.View.new (self.model)
        self.view.connect("individual-activated", self.individual_activated_cb,
                          None)
        self.view.connect("popup-individual-menu",
                          self.popup_individual_menu_cb, None)
        self.view.connect("notify::empty", self.empty_cb, None)
        self.view.connect("individual-tooltip", self.individual_tooltip_cb,
                          None)
        self.view.show_offline (False)
        self.view.show_groups (True)

        self.search = EmpathyRoster.LiveSearch.new (self.view)
        self.view.set_roster_live_search (self.search)

        self.scrolled = Gtk.ScrolledWindow()
        self.scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.scrolled.add_with_viewport (self.view)

        self.box.pack_start (self.search, False, True, 0)
        self.box.pack_start (self.scrolled, True, True, 0)
        self.add (self.box)

        self.set_default_size (300, 600)

    def individual_activated_cb(self, view, individual, user_data):
        print individual.__info__
        print "activated"

    def popup_individual_menu_cb (self, view, individual, button, time, user_data):
        menu = Gtk.Menu()
        menu.connect("deactivate", Gtk.Widget.destroy)
        item = Gtk.MenuItem.new_with_label ("test")
        item.show()
        Gtk.MenuShell.append (menu, item)
        menu.attach_to_widget (view, None)
        menu.popup(None, None, None, None, button, time)

    def empty_cb(self, view, spec, user_data):
        if (view.is_empty()):
            print "view is now empty"
        else:
            print "view is no longer empty"

    def individual_tooltip_cb(self, view, individual, keyboard_mode, tooltip,
                              user_data):
        tooltip.set_text ("test")

def main():
    win = TestEmpathyRosterGIR()
    win.connect("delete-event", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == '__main__':
    main()
