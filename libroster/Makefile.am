SUBDIRS = ../libempathy-gtk/egg-list-box \
	../libempathy-gtk

include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =                                           \
	$(ERROR_CFLAGS)					\
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	-DGCR_API_SUBJECT_TO_CHANGE			\
	$(EMPATHY_CFLAGS)				\
	$(YELL_CFLAGS)					\
	$(ENCHANT_CFLAGS)				\
	$(LIBCHAMPLAIN_CFLAGS)				\
	$(GEOCLUE_CFLAGS)				\
	$(GEOCODE_CFLAGS)				\
	$(MEEGO_CFLAGS)					\
	$(CHEESE_CFLAGS)				\
	$(EGGLISTBOX_CFLAGS)					\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)

libroster_handwritten_source = \
	empathy-roster-utils.c \
	empathy-roster-ui-utils.c \
	empathy-roster-live-search.c \
	empathy-roster-contact.c \
	empathy-roster-group.c \
	empathy-roster-model.c \
	empathy-roster-model-aggregator.c \
	empathy-roster-view.c \
	$(NULL)

libroster_headers = \
	empathy-roster-images.h \
	empathy-roster-utils.h \
	empathy-roster-ui-utils.h \
	empathy-roster-live-search.h \
	empathy-roster-contact.h \
	empathy-roster-group.h \
	empathy-roster-model.h \
	empathy-roster-model-aggregator.h \
	empathy-roster-view.h \
	$(NULL)

pkglib_LTLIBRARIES = libroster.la

libroster_la_LDFLAGS = \
	-no-undefined \
	-release $(VERSION) \
	$(NULL)

libroster_la_SOURCES = \
	$(libroster_handwritten_source) \
	$(libroster_headers) \
	$(NULL)

nodist_libroster_la_SOURCES = \
	$(BUILT_SOURCES)

libroster_la_LIBADD = \
	$(EMPATHY_LIBS) \
	$(EMPATHY_LIBS) \
	$(GTK_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(CANBERRA_LIBS) \
	$(ENCHANT_LIBS) \
	$(LIBCHAMPLAIN_LIBS) \
	$(GEOCLUE_LIBS) \
	$(GEOCODE_LIBS) \
	$(GCR_LIBS) \
	$(MEEGO_LIBS) \
	$(CHEESE_LIBS) \
	$(top_builddir)/libempathy-gtk/egg-list-box/libegglistbox.la \
	$(top_builddir)/libempathy/libempathy.la \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la

check_c_sources = \
	$(libroster_handwritten_source) \
	$(libroster_headers)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

empathy-gtk-enum-types.h: stamp-empathy-gtk-enum-types.h
	$(AM_V_GEN)true
stamp-empathy-gtk-enum-types.h: Makefile $(libempathy_gtk_headers)
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __LIBEMPATHY_GTK_ENUM_TYPES_H__\n" \
			--fhead "#define __LIBEMPATHY_GTK_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __LIBEMPATHY_GTK_ENUM_TYPES_H__ */\n" \
			--fprod "#include <libempathy-gtk/@filename@>\n" \
			--eprod "#define EMPATHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(libempathy_gtk_headers) ) > xgen-gth \
	&& (cmp -s xgen-gth empathy-gtk-enum-type.h || cp xgen-gth empathy-gtk-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

empathy-gtk-enum-types.c: Makefile $(libempathy_gtk_headers)
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"empathy-gtk-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libempathy_gtk_headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

CLEANFILES = \
	$(BUILT_SOURCES) \
	stamp-empathy-gtk-enum-types.h
