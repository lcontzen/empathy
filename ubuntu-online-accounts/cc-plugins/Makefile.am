# Generated using empathy/ubuntu-online-accounts/cc-plugins/generate-plugins.py
# Do NOT edit manually
SUBDIRS = providers services

plugindir = $(ACCOUNTS_PROVIDER_PLUGIN_DIR)

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
	libjabber.la \
	$(NULL)

libjabber_la_SOURCES = \
	empathy-accounts-plugin.c \
	empathy-accounts-plugin.h \
	empathy-accounts-plugin-widget.c \
	empathy-accounts-plugin-widget.h
libjabber_la_LDFLAGS = -module -avoid-version
libjabber_la_LIBADD = \
	$(UOA_LIBS)					\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la
