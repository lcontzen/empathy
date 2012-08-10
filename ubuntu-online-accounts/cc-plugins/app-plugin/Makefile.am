plugindir = $(ACCOUNTS_APP_PLUGIN_DIR)

INCLUDES =					\
	-I$(top_builddir)			\
	-I$(top_srcdir)				\
	-DLOCALEDIR=\""$(datadir)/locale"\"	\
	$(UOA_CFLAGS)				\
	$(WARN_CFLAGS)				\
	$(ERROR_CFLAGS)				\
	$(DISABLE_DEPRECATED)			\
	$(EMPATHY_CFLAGS)

plugin_LTLIBRARIES = \
	libempathy.la\
	$(NULL)

libempathy_la_SOURCES = \
	empathy-app-plugin.c \
	empathy-app-plugin.h \
	empathy-app-plugin-widget.c \
	empathy-app-plugin-widget.h
libempathy_la_LDFLAGS = -module -avoid-version
libempathy_la_LIBADD = \
	$(UOA_LIBS)					\
	$(top_builddir)/libempathy/libempathy.la \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la
