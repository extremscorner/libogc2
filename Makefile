#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

# GNU make treats spaces as separators in variable-expanded file names. Keep
# exported paths readable, and escape them only where make parses a path list.
empty :=
space := $(empty) $(empty)
escape = $(subst $(space),\ ,$(1))

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

export PATH	:=	$(DEVKITPPC)/bin:$(PATH)

export LIBOGC_MAJOR	:= 2
export LIBOGC_MINOR	:= 1
export LIBOGC_PATCH	:= 0

include	$(call escape,$(DEVKITPPC))/base_rules

DATESTRING	:=	$(shell date -u +%Y%m%d)
VERSTRING	:=	$(shell printf "r%s.%s" "$$(git rev-list --count HEAD)" "$$(git rev-parse --short=7 HEAD)")

#---------------------------------------------------------------------------------
ifeq ($(strip $(PLATFORM)),)
#---------------------------------------------------------------------------------
export BASEDIR		:= $(CURDIR)
export LWIPDIR		:= $(BASEDIR)/lwip
export OGCDIR		:= $(BASEDIR)/libogc
export MODDIR		:= $(BASEDIR)/libmodplay
export DBDIR		:= $(BASEDIR)/libdb
export DIDIR		:= $(BASEDIR)/libdi
export BTEDIR		:= $(BASEDIR)/lwbt
export WIIUSEDIR	:= $(BASEDIR)/wiiuse
export TINYSMBDIR	:= $(BASEDIR)/libtinysmb
export LIBASNDDIR	:= $(BASEDIR)/libasnd
export LIBAESNDDIR	:= $(BASEDIR)/libaesnd
export LIBISODIR	:= $(BASEDIR)/libiso9660
export LIBWIIKEYB	:= $(BASEDIR)/libwiikeyboard
export LIBCDIR		:= $(BASEDIR)/libc
export BUILD		:=	$(BASEDIR)/build
export DEPS			:=	$(BASEDIR)/deps
export LIBS			:=	$(BASEDIR)/lib

export INCDIR		:=	$(BASEDIR)/include

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

LIBDIR		:=	$(call escape,$(LIBS)/$(PLATFORM))
DEPSDIR		:=	$(call escape,$(DEPS)/$(PLATFORM))

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------


#---------------------------------------------------------------------------------
BBALIB		:= $(LIBDIR)/libbba
OGCLIB		:= $(LIBDIR)/libogc
MODLIB		:= $(LIBDIR)/libmodplay
DBLIB		:= $(LIBDIR)/libdb
DILIB		:= $(LIBDIR)/libdi
BTELIB		:= $(LIBDIR)/libbte
WIIUSELIB	:= $(LIBDIR)/libwiiuse
TINYSMBLIB	:= $(LIBDIR)/libtinysmb
ASNDLIB		:= $(LIBDIR)/libasnd
AESNDLIB	:= $(LIBDIR)/libaesnd
ISOLIB		:= $(LIBDIR)/libiso9660
WIIKEYBLIB	:= $(LIBDIR)/libwiikeyboard

#---------------------------------------------------------------------------------
DEFINCS		:= -I$(call escape,$(BASEDIR)) -I$(call escape,$(INCDIR))
INCLUDES	:=	$(DEFINCS) -I$(call escape,$(LWIPDIR)/include) -I$(call escape,$(LWIPDIR)/include/ipv4) -I$(call escape,$(LWIPDIR)/include/netif) \
				-I$(call escape,$(INCDIR)/ogc) -I$(call escape,$(INCDIR)/ogc/machine) \
				-I$(call escape,$(INCDIR)/modplay) \
				-I$(call escape,$(INCDIR)/bte) \
				-I$(call escape,$(INCDIR)/sdcard)

MACHDEP		:= -DGEKKO -mcpu=750 -meabi -msdata=sysv -mhard-float -ffunction-sections -fdata-sections


ifeq ($(PLATFORM),wii)
INCLUDES	+=	-I$(call escape,$(BUILD)/wii)
MACHDEP		+=	-DHW_RVL -Wa,-mbroadway
endif

ifeq ($(PLATFORM),cube)
INCLUDES	+=	-I$(call escape,$(BUILD)/cube)
MACHDEP		+=	-DHW_DOL -Wa,-mgekko
endif

INCLUDES	+=	-I$(call escape,$(PORTLIBS_PATH)/ppc/include)


CFLAGS		:= -DLIBOGC_INTERNAL -g -O2 -fno-strict-aliasing -Wall -Wno-address-of-packed-member -Wno-prio-ctor-dtor $(MACHDEP) $(INCLUDES)
ASFLAGS		:=	$(MACHDEP) -mregnames -D_LANGUAGE_ASSEMBLY $(INCLUDES)

#---------------------------------------------------------------------------------
VPATH_ROOT := $(if $(strip $(PLATFORM)),../..,$(BASEDIR))
VPATH :=	$(call escape,$(VPATH_ROOT)/lwip)			\
			$(call escape,$(VPATH_ROOT)/lwip/arch/gc)	\
			$(call escape,$(VPATH_ROOT)/lwip/arch/gc/netif) \
			$(call escape,$(VPATH_ROOT)/lwip/core)		\
			$(call escape,$(VPATH_ROOT)/lwip/core/ipv4)	\
			$(call escape,$(VPATH_ROOT)/lwip/netif)	\
			$(call escape,$(VPATH_ROOT)/libogc)		\
			$(call escape,$(VPATH_ROOT)/libmodplay)	\
			$(call escape,$(VPATH_ROOT)/libdb)		\
			$(call escape,$(VPATH_ROOT)/libdb/uIP)	\
			$(call escape,$(VPATH_ROOT)/libdi)		\
			$(call escape,$(VPATH_ROOT)/lwbt)		\
			$(call escape,$(VPATH_ROOT)/wiiuse)		\
			$(call escape,$(VPATH_ROOT)/libtinysmb)	\
			$(call escape,$(VPATH_ROOT)/libasnd)		\
			$(call escape,$(VPATH_ROOT)/libaesnd)	\
			$(call escape,$(VPATH_ROOT)/libiso9660)	\
			$(call escape,$(VPATH_ROOT)/libwiikeyboard)	\
			$(call escape,$(VPATH_ROOT)/libc)


#---------------------------------------------------------------------------------
LWIPOBJ		:=	network.o netio.o gcif.o	\
			inet.o mem.o dhcp.o raw.o		\
			memp.o netif.o pbuf.o stats.o	\
			sys.o tcp.o tcp_in.o tcp_out.o	\
			udp.o icmp.o ip.o ip_frag.o		\
			ip_addr.o etharp.o loopif.o		\
			enc28j60if.o w5500if.o w6x00if.o

#---------------------------------------------------------------------------------
OGCOBJ		:=	\
			console.o  lwp_priority.o lwp_queue.o lwp_threadq.o lwp_threads.o lwp_sema.o	\
			lwp_messages.o lwp.o lwp_handler.o lwp_stack.o lwp_mutex.o 	\
			lwp_watchdog.o lwp_wkspace.o lwp_objmgr.o lwp_heap.o sys_state.o \
			exception_handler.o exception.o irq.o irq_handler.o semaphore.o \
			video_asm.o video.o pad.o dvd.o exi.o mutex.o arqueue.o	arqmgr.o	\
			cache_asm.o system.o system_alarm.o system_asm.o cond.o \
			gx.o gu.o gu_psasm.o audio.o cache.o decrementer.o			\
			message.o card.o aram.o depackrnc.o decrementer_handler.o	\
			depackrnc1.o dsp.o si.o si_steering.o tpl.o ipc.o ogc_crt0.o \
			console_font_8x16.o timesupp.o lock_supp.o newlibc.o usbgecko.o usbmouse.o \
			sbrk.o kprintf.o stm.o ios.o es.o isfs.o usb.o network_common.o \
			sdgecko_io.o sdgecko_buf.o gcsd.o argv.o network_wii.o wiisd.o conf.o usbstorage.o \
			texconv.o wiilaunch.o mic.o system_report.o mmce.o n64.o threads_supp.o \
			malloc_wii.o malloc.o mallocr.o strdup.o strndup.o

#---------------------------------------------------------------------------------
MODOBJ		:=	freqtab.o mixer.o modplay.o semitonetab.o gcmodplay.o

#---------------------------------------------------------------------------------
DBOBJ		:=	uip_ip.o uip_tcp.o uip_pbuf.o uip_netif.o uip_arp.o uip_arch.o \
				uip_icmp.o memb.o memr.o bba.o tcpip.o debug.o debug_handler.o \
				debug_supp.o geckousb.o
#---------------------------------------------------------------------------------
DIOBJ		:=	di.o

#---------------------------------------------------------------------------------
BTEOBJ		:=	bte.o hci.o l2cap.o btmemb.o btmemr.o btpbuf.o physbusif.o

#---------------------------------------------------------------------------------
WIIUSEOBJ	:=	classic.o dynamics.o events.o guitar_hero_3.o io.o io_wii.o ir.o \
				nunchuk.o wiiboard.o wiiuse.o speaker.o wpad.o motion_plus.o

#---------------------------------------------------------------------------------
TINYSMBOBJ	:=	des.o md4.o ntlm.o smb.o smb_devoptab.o

#---------------------------------------------------------------------------------
ASNDLIBOBJ	:=	asndlib.o mp3player.o

#---------------------------------------------------------------------------------
AESNDLIBOBJ	:=	aesndlib.o aesndmp3player.o

#---------------------------------------------------------------------------------
ISOLIBOBJ	:=	iso9660.o

#---------------------------------------------------------------------------------
WIIKEYBLIBOBJ	:=	usbkeyboard.o keyboard.o ukbdmap.o wskbdutil.o



all: wii cube

#---------------------------------------------------------------------------------
wii: include/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d "$(LIBS)/wii" ] || mkdir -p "$(LIBS)/wii"
	@[ -d "$(DEPS)/wii" ] || mkdir -p "$(DEPS)/wii"
	@[ -d "$(BUILD)/wii" ] || mkdir -p "$(BUILD)/wii"
	@$(MAKE) PLATFORM=wii libs -C "$(BUILD)/wii" -f "$(CURDIR)/Makefile"

#---------------------------------------------------------------------------------
cube: include/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d "$(LIBS)/cube" ] || mkdir -p "$(LIBS)/cube"
	@[ -d "$(DEPS)/cube" ] || mkdir -p "$(DEPS)/cube"
	@[ -d "$(BUILD)/cube" ] || mkdir -p "$(BUILD)/cube"
	@$(MAKE) PLATFORM=cube libs -C "$(BUILD)/cube" -f "$(CURDIR)/Makefile"


#---------------------------------------------------------------------------------
include/ogc/libversion.h: .git/HEAD .git/index Makefile
#---------------------------------------------------------------------------------
	@echo "#ifndef __OGC_LIBVERSION_H__" > $@
	@echo "#define __OGC_LIBVERSION_H__" >> $@
	@echo >> $@
	@echo "#define _V_MAJOR_	$(LIBOGC_MAJOR)" >> $@
	@echo "#define _V_MINOR_	$(LIBOGC_MINOR)" >> $@
	@echo "#define _V_PATCH_	$(LIBOGC_PATCH)" >> $@
	@echo >> $@
	@echo "#define _V_DATE_			$$(git log -1 --format=%cd --date=format-local:'"%b %e %Y"')" >> $@
	@echo "#define _V_TIME_			$$(git log -1 --format=%cd --date=format-local:'"%H:%M:%S"')" >> $@
	@echo >> $@
	@echo '#define _V_STRING "libogc2 '$(VERSTRING)'"' >> $@
	@echo >> $@
	@echo "#define _LIBOGC2_REVISION_	$$(git rev-list --count HEAD)" >> $@
	@echo >> $@
	@echo "#endif // __OGC_LIBVERSION_H__" >> $@

#---------------------------------------------------------------------------------
asndlib.o: asnd_dsp_mixer.h
#---------------------------------------------------------------------------------
aesndlib.o: aesnddspmixer.h
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
asnd_dsp_mixer.h: $(call escape,$(LIBASNDDIR))/dsp_mixer/dsp_mixer.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@gcdsptool -c "$<" -o "$@"

#---------------------------------------------------------------------------------
aesnddspmixer.h: $(call escape,$(LIBAESNDDIR))/dspcode/dspmixer.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@gcdsptool -c "$<" -o "$@"

#---------------------------------------------------------------------------------
$(BBALIB).a: $(LWIPOBJ)
#---------------------------------------------------------------------------------
$(OGCLIB).a: $(OGCOBJ)
#---------------------------------------------------------------------------------
$(MODLIB).a: $(MODOBJ)
#---------------------------------------------------------------------------------
$(DBLIB).a: $(DBOBJ)
#---------------------------------------------------------------------------------
$(DILIB).a: $(DIOBJ)
#---------------------------------------------------------------------------------
$(TINYSMBLIB).a: $(TINYSMBOBJ)
#---------------------------------------------------------------------------------
$(ASNDLIB).a: $(ASNDLIBOBJ)
#---------------------------------------------------------------------------------
$(AESNDLIB).a: $(AESNDLIBOBJ)
#---------------------------------------------------------------------------------
$(ISOLIB).a: $(ISOLIBOBJ)
#---------------------------------------------------------------------------------
$(WIIKEYBLIB).a: $(WIIKEYBLIBOBJ)
#---------------------------------------------------------------------------------
$(BTELIB).a: $(BTEOBJ)
#---------------------------------------------------------------------------------
$(WIIUSELIB).a: $(WIIUSEOBJ)
#---------------------------------------------------------------------------------

.PHONY: libs wii cube install uninstall docs docker

#---------------------------------------------------------------------------------
install: wii cube
#---------------------------------------------------------------------------------
	@mkdir -p "$(DESTDIR)$(DEVKITPRO)/libogc2/gamecube/lib"
	@mkdir -p "$(DESTDIR)$(DEVKITPRO)/libogc2/wii/lib"
	@cp -frv include "$(DESTDIR)$(DEVKITPRO)/libogc2/gamecube"
	@cp -frv include "$(DESTDIR)$(DEVKITPRO)/libogc2/wii"
	@cp -frv lib/cube/*.a "$(DESTDIR)$(DEVKITPRO)/libogc2/gamecube/lib"
	@cp -frv lib/wii/*.a "$(DESTDIR)$(DEVKITPRO)/libogc2/wii/lib"
	@cp -frv *_license.txt "$(DESTDIR)$(DEVKITPRO)/libogc2"
	@cp -frv *_rules "$(DESTDIR)$(DEVKITPRO)/libogc2"

#---------------------------------------------------------------------------------
uninstall:
#---------------------------------------------------------------------------------
	@rm -frv "$(DESTDIR)$(DEVKITPRO)/libogc2"


LIBRARIES	:=	$(OGCLIB).a  $(MODLIB).a $(DBLIB).a $(TINYSMBLIB).a $(ASNDLIB).a $(AESNDLIB).a $(ISOLIB).a

ifeq ($(PLATFORM),cube)
LIBRARIES	+=	$(BBALIB).a
endif
ifeq ($(PLATFORM),wii)
LIBRARIES	+=	$(BTELIB).a $(WIIUSELIB).a $(DILIB).a $(WIIKEYBLIB).a
endif

# base_rules' generic archive recipe leaves $@ unquoted. Keep the archive
# target safe when the checkout or output directory contains spaces.
$(LIBRARIES):
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $@)
	$(ADD_COMPILE_COMMAND) end
	$(SILENTCMD)rm -f "$@"
	$(SILENTCMD)$(AR) -rc "$@" $^

#---------------------------------------------------------------------------------
libs: $(LIBRARIES)
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr "$(BUILD)"
	rm -fr "$(DEPS)"
	rm -fr "$(LIBS)"

#---------------------------------------------------------------------------------
docs:
#---------------------------------------------------------------------------------
	VERSTRING="$(VERSTRING)" doxygen Doxyfile

#---------------------------------------------------------------------------------
docker:
#---------------------------------------------------------------------------------
	docker build --no-cache -t ghcr.io/extremscorner/libogc2:$(DATESTRING) -t ghcr.io/extremscorner/libogc2:latest .

-include $(call escape,$(DEPSDIR))/*.d
