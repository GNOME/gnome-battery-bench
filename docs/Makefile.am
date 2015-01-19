CLEANFILES =
EXTRA_DIST =

if BUILD_DOCS
man1_MANS = gbb.1
endif

EXTRA_DIST += asciidoc.conf gbb.txt
CLEANFILES += gbb.html gbb.1

%.xml: %.txt
	$(AM_V_GEN) $(ASCIIDOC) -f $(srcdir)/asciidoc.conf -d manpage -b docbook -o $@ $<

%.html: %.txt
	$(AM_V_GEN) $(ASCIIDOC) -f $(srcdir)/asciidoc.conf -d manpage -o $@ $<

%.1: %.xml
	$(AM_V_GEN) $(XMLTO) man $<

