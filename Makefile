builddir = build
common_objs =  $(builddir)/common-common.o $(builddir)/network-network.o
consumer_objs = $(builddir)/consumer-main.o $(common_objs)
server_objs = $(builddir)/server-main.o $(builddir)/server-listening.o $(common_objs)
provider_objs = $(builddir)/provider-main.o $(common_objs)
link_options = -lcrypto
comp_options = -O2 --std=gnu11 -Wall -Wno-unknown-pragmas
options = $(link_options) $(comp_options)

.PHONY : all consumer server provider clean
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


$(builddir)/network-%.o : network/%.c
	cc -c $(comp_options) $< -o $@
$(builddir)/common-%.o : common/%.c
	cc -c $(comp_options) $< -o $@

$(builddir)/consumer-%.o : consumer/%.c
	cc -c $(comp_options) $< -o $@
$(builddir)/server-%.o : server/%.c
	cc -c $(comp_options) $< -o $@
$(builddir)/provider-%.o : provider/%.c
	cc -c $(comp_options) $< -o $@

install : consumer server provider

clean : 
	-rm -f $(consumer_objs) $(server_objs) $(provider_objs) $(builddir)/consumer $(builddir)/server $(builddir)/provider

uninstall : 

