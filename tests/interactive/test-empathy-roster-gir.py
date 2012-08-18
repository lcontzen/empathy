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

    def individual_activated_cb(view, individual, user_data):
        print individual + "activated"

def main():
    win = TestEmpathyRosterGIR()
    win.connect("delete-event", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == '__main__':
    main()
