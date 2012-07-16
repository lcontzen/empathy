AM_CPPFLAGS = \
        $(UOA_CFLAGS) \
        $(ERROR_CFLAGS)

pluginsdir = $(MISSION_CONTROL_PLUGINS_DIR)
plugins_LTLIBRARIES = \
        mcp-account-manager-uoa.la

mcp_account_manager_uoa_la_SOURCES = \
        mission-control-plugin.c \
        mcp-account-manager-uoa.c mcp-account-manager-uoa.h \
	$(NULL)

mcp_account_manager_uoa_la_LIBADD = \
        $(UOA_LIBS)

mcp_account_manager_uoa_la_LDFLAGS = \
        -module \
        -avoid-version
