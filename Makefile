
CFLAGS = -c -IC: -throwback -zM -Wp
LINKFLAGS = -rmf -o $@
ASFLAGS = -throwback
CMHGFLAGS = -d pcapmod.h -throwback

LIBS = C:o.stubs

pcap: pcap.o pcapmod.o pcapasm.o
	link $(LINKFLAGS) pcap.o pcapmod.o pcapasm.o $(LIBS)

%.o: %.s
	objasm $(ASFLAGS) -o $@ $<

%.o: %.c
	cc $(CFLAGS) -o $@ $<

pcap.o: pcapmod.o

pcapmod.o:   cmhg.pcapmod
	cmhg $(CMHGFLAGS)  cmhg.pcapmod -o pcapmod.o

