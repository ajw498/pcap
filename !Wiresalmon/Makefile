
CFLAGS = -c -IC: -throwback -zM -Wp -fah
LINKFLAGS = -rmf -o $@
ASFLAGS = -throwback
CMHGFLAGS = -d wiresalmonmod.h -throwback

DISTCC = distcc

CXX = $(DISTCC) g++

CXXFLAGS = -Irtk: -Wall -mthrowback -mlibscl -O1 -mpoke-function-name

LIBS = C:o.stubs

all: Wiresalmon !RunImage

Wiresalmon: wiresalmon.o wiresalmonmod.o wiresalmonasm.o
	link $(LINKFLAGS) wiresalmon.o wiresalmonmod.o wiresalmonasm.o $(LIBS)

!RunImage: frontend.o
	$(CXX) $(CXXFLAGS) frontend.o -o $@ -lrtk:a.rtk

%.o: %.s
	objasm $(ASFLAGS) -o $@ $<

%.o: %.c
	cc $(CFLAGS) -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

wiresalmon.o: wiresalmonmod.o

wiresalmonmod.o:   cmhg.wiresalmonmod
	cdir o
	cmhg $(CMHGFLAGS)  cmhg.wiresalmonmod -o wiresalmonmod.o

