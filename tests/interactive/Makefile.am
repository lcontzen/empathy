AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-I$(top_srcdir)                          	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)

LDADD =								\
	$(top_builddir)/libroster/libroster.la \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(EMPATHY_LIBS)

noinst_PROGRAMS =			\
	empathy-logs			\
	test-empathy-contact-blocking-dialog \
	test-empathy-presence-chooser	\
	test-empathy-status-preset-dialog \
	test-empathy-protocol-chooser \
	test-empathy-account-chooser \
	test-empathy-calendar-button \
	test-empathy-roster-view \
	test-empathy-dual-roster-view \
	test-empathy-roster-model-aggregator

empathy_logs_SOURCES = empathy-logs.c
test_empathy_contact_blocking_dialog_SOURCES = test-empathy-contact-blocking-dialog.c
test_empathy_presence_chooser_SOURCES = test-empathy-presence-chooser.c
test_empathy_status_preset_dialog_SOURCES = test-empathy-status-preset-dialog.c
test_empathy_protocol_chooser_SOURCES = test-empathy-protocol-chooser.c
test_empathy_account_chooser_SOURCES = test-empathy-account-chooser.c
test_empathy_calendar_button_SOURCES = test-empathy-calendar-button.c
test_empathy_roster_view_SOURCES = test-empathy-roster-view.c
test_empathy_dual_roster_view_SOURCES = test-empathy-dual-roster-view.c
test_empathy_roster_model_aggregator_SOURCES = test-empathy-roster-model-aggregator.c
