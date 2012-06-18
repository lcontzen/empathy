themesdir = $(datadir)/adium/message-styles

themes = \
	$(boxes_messageStyle)
	$(NULL)

boxes_messageStyle = \
	Boxes.AdiumMessageStyle/Contents/Resources/Incoming/Content.html \
	Boxes.AdiumMessageStyle/Contents/Resources/Incoming/NextContent.html \
	Boxes.AdiumMessageStyle/Contents/Resources/main.css \
	Boxes.AdiumMessageStyle/Contents/Resources/Status.html \
	Boxes.AdiumMessageStyle/Contents/Resources/Variants/Clean.css \
	Boxes.AdiumMessageStyle/Contents/Resources/Variants/Simple.css \
	Boxes.AdiumMessageStyle/Contents/Resources/Variants/Blue.css \
	Boxes.AdiumMessageStyle/Contents/Info.plist \
	$(NULL)

install-themes: $(themes)
	for f in $^; do \
		mkdir -p `dirname $(DESTDIR)$(themesdir)/$$f`; \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(themesdir)/$$f; \
	done

uninstall-themes: $(themes)
	for f in $^; do \
		rm -f $(DESTDIR)$(themesdir)/$$f; \
	done

install-data-local: install-themes

uninstall-local: uninstall-themes

EXTRA_DIST = \
	$(themes) \
	$(NULL)
