CC=cc
FC=ftn
CXX=CC

CCFLAGS=-O3
FCFLAGS=-O3
CXXFLAGS=-O3

LDFLAGS=

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .o .c .f90 .cpp

# some implicit rules
.c.o:
	$(CC) $(CCFLAGS) -c $<

.f90.o:
	$(FC) $(FCFLAGS) -c $<

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<
