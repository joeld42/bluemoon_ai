bin_PROGRAMS = bluemoon
noinst_PROGRAMS = learner condition dumpnet
dist_bin_SCRIPTS = do_matchup

bluemoon_SOURCES = ai.c engine.c init.c net.c gui.c bluemoon.h net.h
learner_SOURCES = ai.c engine.c init.c net.c learner.c bluemoon.h net.h
condition_SOURCES = net.c condition.c net.h
dumpnet_SOURCES = net.c dumpnet.c init.c engine.c bluemoon.h net.h


dist_pkgdata_DATA = cards.txt
bluemoon_CFLAGS = -Wall @GTK_CFLAGS@ -DLOCALEDIR=\"$(localedir)\" -DDATADIR=\"$(pkgdatadir)\" 
bluemoon_LDADD = @GTK_LIBS@ @LIBINTL@

learner_LDADD = @LIBINTL@
condition_LDADD = @LIBINTL@
dumpnet_LDADD = @LIBINTL@

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath m4/ChangeLog

SUBDIRS = image networks po
