noinst_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD   = libtopodisc.la

lib_LTLIBRARIES = libtopodisc.la 
libtopodisc_la_SOURCES = libtopodisc.cpp libtopodisc.hpp findcliques.c findcliques.h meshmap2d.c
include_HEADERS = findcliques.h

# Documentation
# 
manfiles =  MPE_FindCliqueFromName.3 MPE_CreateSMPComm.3 \
	MPE_CreateRoundRobinComm.3 MPE_CreateConseqComm.3 \
	MPE_FindCliqueFromNodename.3
wwwfiles =  MPE_FindCliqueFromName.htm MPE_CreateSMPComm.htm \
	MPE_CreateRoundRobinComm.htm MPE_CreateConseqComm.htm \
	MPE_FindCliqueFromNodename.htm
man3_MANS = $(manfiles)

#
# doctext is a special (and venerable) program from the sowing collection
# http://www.cs.uiuc.edu/~wgropp/projects/software/sowing/index.htm
# We may change to a newer tool at some time
DOCTEXT = @DOCTEXT@
$(manfiles): findcliques.c
	$(DOCTEXT) -man -ext 3 $<
$(wwwfiles): findcliques.c
	$(DOCTEXT) -html $<

dist_man_MANS = $(manfiles)