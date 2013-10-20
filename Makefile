builddir = build
common_objs =  $(builddir)/common-common.o $(builddir)/network-network.o
consumer_objs = $(builddir)/consumer-main.o $(common_objs)
server_objs = $(builddir)/consumer-main.o $(builddir)/server-listening.o $(common_objs)
provider_objs = $(builddir)/provider-main.o $(common_objs)
link_options = -lcrypto
comp_options = -O2 --std=gnu11 -Wall -Wno-unknown-pragmas
options = $(link_options) $(comp_options)

.PHONY : all consumer server provider
all : $(builddir)/consumer $(builddir)/server $(builddir)/provider

consumer : $(builddir)/consumer
server : $(builddir)/server
provider : $(builddir)/provider

$(builddir)/consumer : $(consumer_objs)
	cc -o $(builddir)/consumer $(consumer_objs) $(options)

$(builddir)/server : $(server_objs)
	cc -o $(builddir)/server $(server_objs) $(options)

$(builddir)/provider : $(provider_objs)
	cc -o $(builddir)/provider $(provider_objs) $(options)

$(builddir)/consumer-main.o : 
	cc -o $(builddir)/consumer-main.o -c consumer/main.c $(comp_options)
$(builddir)/network-network.o : 
	cc -o $(builddir)/network-network.o -c network/network.c $(comp_options)
$(builddir)/common-common.o : 
	cc -o $(builddir)/common-common.o -c common/common.c $(comp_options)
$(builddir)/provider-main.o : 
	cc -o $(builddir)/provider-main.o -c provider/main.c $(comp_options)
$(builddir)/server-listening.o : 
	cc -o $(builddir)/server-listening.o -c server/listening.c $(comp_options)
$(builddir)/server-main.o : 
	cc -o $(builddir)/server-main.o -c server/main.c $(comp_options)

install : 

.PHONY : clean
clean : 
	-rm -f $(consumer_objs) $(server_objs) $(provider_objs) $(builddir)/consumer $(builddir)/server $(builddir)/provider

uninstall : 

