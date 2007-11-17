
CFLAGS = -c -IC: -throwback -zM -Wp
LINKFLAGS = -rmf -o $@
ASFLAGS = -throwback
CMHGFLAGS = -d pcapmod.h -throwback

DISTCC = distcc

CXX = $(DISTCC) g++

CXXFLAGS = -Irtk: -Wall -mthrowback -mlibscl -O1 -mpoke-function-name

LIBS = C:o.stubs

all: pcap !RunImage

pcap: pcap.o pcapmod.o pcapasm.o
	link $(LINKFLAGS) pcap.o pcapmod.o pcapasm.o $(LIBS)

!RunImage: frontend.o
	$(CXX) $(CXXFLAGS) frontend.o -o $@ -lrtk:a.rtk

%.o: %.s
	objasm $(ASFLAGS) -o $@ $<

%.o: %.c
	cc $(CFLAGS) -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

pcap.o: pcapmod.o

pcapmod.o:   cmhg.pcapmod
	cmhg $(CMHGFLAGS)  cmhg.pcapmod -o pcapmod.o

