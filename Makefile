#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(INSTALL_PREFIX)),)
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif
# Prevent variable expansion so it stays as-is in the installable rules
INSTALL_PREFIX	:= $$(DEVKITPRO)/libogc2
endif

INCDEST	?= include
LIBDEST	?= lib

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

CURFILE		:=	$(abspath $(lastword $(MAKEFILE_LIST)))

export PATH	:=	$(DEVKITPPC)/bin:$(PATH)

export LIBOGC_MAJOR	:= 2
export LIBOGC_MINOR	:= 1
export LIBOGC_PATCH	:= 0

include	$(DEVKITPPC)/base_rules
export AR	:=	$(AR) -D

BUILD		:=	build

VERSTRING	:=	$(shell printf "r%s.%s" "$$(git rev-list --count HEAD)" "$$(git rev-parse --short=7 HEAD)")

#---------------------------------------------------------------------------------
ifeq ($(strip $(PLATFORM)),)
#---------------------------------------------------------------------------------
export BUILDDIR		:= $(CURDIR)
export BASEDIR		:= $(dir $(CURFILE))
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
export DEPS			:=	$(BUILDDIR)/deps
export LIBS			:=	$(BUILDDIR)/lib

export INCDIR		:=	$(BUILDDIR)/include

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

export LIBDIR		:= $(LIBS)/$(PLATFORM)
export DEPSDIR		:=	$(DEPS)/$(PLATFORM)

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
DEFINCS		:= -I$(BASEDIR) -I$(BASEDIR)/gc
INCLUDES	:=	$(DEFINCS) -I$(BASEDIR)/gc/netif -I$(BASEDIR)/gc/ipv4 \
				-I$(BASEDIR)/gc/ogc -I$(BASEDIR)/gc/ogc/machine \
				-I$(BUILDDIR)/gc/ogc \
				-I$(BASEDIR)/gc/modplay \
				-I$(BASEDIR)/gc/bte \
				-I$(BASEDIR)/gc/sdcard -I$(BASEDIR)/gc/wiiuse \
				-I$(BASEDIR)/gc/di \
				-I$(CURDIR)

MACHDEP		:= -DBIGENDIAN -DGEKKO -mcpu=750 -meabi -msdata=eabi -mhard-float -ffunction-sections -fdata-sections


ifeq ($(PLATFORM),wii)
INCLUDES	+=	-I$(BASEDIR)/wii \
				-I$(PORTLIBS_PATH)/wii/include
MACHDEP		+=	-DHW_RVL -Wa,-mbroadway
endif

ifeq ($(PLATFORM),cube)
INCLUDES	+=	-I$(BASEDIR)/cube \
				-I$(PORTLIBS_PATH)/gamecube/include
MACHDEP		+=	-DHW_DOL -Wa,-mgekko
endif

INCLUDES	+=	-I$(PORTLIBS_PATH)/ppc/include


OPTLEVEL	?= 2
CFLAGS		:= -DLIBOGC_INTERNAL -g -O$(OPTLEVEL) -fno-strict-aliasing -Wall -Wno-address-of-packed-member $(MACHDEP) $(INCLUDES)
ASFLAGS		:=	$(MACHDEP) -mregnames -D_LANGUAGE_ASSEMBLY $(INCLUDES)

#---------------------------------------------------------------------------------
VPATH :=	$(LWIPDIR)				\
			$(LWIPDIR)/arch/gc		\
			$(LWIPDIR)/arch/gc/netif	\
			$(LWIPDIR)/core			\
			$(LWIPDIR)/core/ipv4	\
			$(LWIPDIR)/netif	\
			$(OGCDIR)			\
			$(MODDIR)			\
			$(DBDIR)			\
			$(DBDIR)/uIP		\
			$(DIDIR)		\
			$(BTEDIR)		\
			$(WIIUSEDIR)		\
			$(TINYSMBDIR)		\
			$(LIBASNDDIR)		\
			$(LIBAESNDDIR)		\
			$(LIBISODIR)		\
			$(LIBWIIKEYB)


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
			depackrnc1.o dsp.o si.o tpl.o ipc.o ogc_crt0.o \
			console_font_8x16.o timesupp.o lock_supp.o newlibc.o usbgecko.o usbmouse.o \
			sbrk.o malloc_lock.o kprintf.o stm.o ios.o es.o isfs.o usb.o network_common.o \
			sdgecko_io.o sdgecko_buf.o gcsd.o argv.o network_wii.o wiisd.o conf.o usbstorage.o \
			texconv.o wiilaunch.o mic.o system_report.o

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



PLATFORMS	?= wii cube
all: $(PLATFORMS)

#---------------------------------------------------------------------------------
wii: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d $(INCDIR) ] || mkdir -p $(INCDIR)
	@[ -d $(LIBS)/wii ] || mkdir -p $(LIBS)/wii
	@[ -d $(DEPS)/wii ] || mkdir -p $(DEPS)/wii
	@[ -d wii ] || mkdir -p wii
	@$(MAKE) PLATFORM=wii libs -C wii -f $(CURFILE)

#---------------------------------------------------------------------------------
cube: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d $(INCDIR) ] || mkdir -p $(INCDIR)
	@[ -d $(LIBS)/cube ] || mkdir -p $(LIBS)/cube
	@[ -d $(DEPS)/cube ] || mkdir -p $(DEPS)/cube
	@[ -d cube ] || mkdir -p cube
	@$(MAKE) PLATFORM=cube libs -C cube -f $(CURFILE)


#---------------------------------------------------------------------------------
gc/ogc/libversion.h : $(CURFILE)
#---------------------------------------------------------------------------------
	@[ -d gc/ogc ] || mkdir -p gc/ogc
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
RULES		:= wii_rules gamecube_rules
%_rules: $(BASEDIR)/%_rules.in
#---------------------------------------------------------------------------------
	@sed \
		-e "s|@PREFIX@|\$(INSTALL_PREFIX)|g" \
		-e "s|@INCDIR@|$(INCDEST)|g" \
		-e "s|@LIBDIR@|$(LIBDEST)|g" \
		$< > $@

#---------------------------------------------------------------------------------
asndlib.o: asnd_dsp_mixer.h
#---------------------------------------------------------------------------------
aesndlib.o: aesnddspmixer.h
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
asnd_dsp_mixer.h: $(LIBASNDDIR)/dsp_mixer/dsp_mixer.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@gcdsptool -c $< -o $@

#---------------------------------------------------------------------------------
aesnddspmixer.h: $(LIBAESNDDIR)/dspcode/dspmixer.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@gcdsptool -c $< -o $@

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

.PHONY: libs wii cube install-headers install uninstall dist docs

#---------------------------------------------------------------------------------
install-headers: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@mkdir -p $(INCDIR)
	@mkdir -p $(INCDIR)/ogc/machine
	@mkdir -p $(INCDIR)/sys
	@mkdir -p $(INCDIR)/bte
	@mkdir -p $(INCDIR)/wiiuse
	@mkdir -p $(INCDIR)/modplay
	@mkdir -p $(INCDIR)/sdcard
	@mkdir -p $(INCDIR)/di
	@mkdir -p $(INCDIR)/wiikeyboard
	@cp $(BASEDIR)/gc/*.h $(INCDIR)
	@cp $(BASEDIR)/gc/ogc/*.h $(INCDIR)/ogc
	@cp $(BUILDDIR)/gc/ogc/*.h $(INCDIR)/ogc
	@cp $(BASEDIR)/gc/ogc/machine/*.h $(INCDIR)/ogc/machine
	@cp $(BASEDIR)/gc/sys/*.h $(INCDIR)/sys
	@cp $(BASEDIR)/gc/bte/*.h $(INCDIR)/bte
	@cp $(BASEDIR)/gc/wiiuse/*.h $(INCDIR)/wiiuse
	@cp $(BASEDIR)/gc/modplay/*.h $(INCDIR)/modplay
	@cp $(BASEDIR)/gc/sdcard/*.h $(INCDIR)/sdcard
	@cp $(BASEDIR)/gc/di/*.h $(INCDIR)/di
	@cp $(BASEDIR)/gc/wiikeyboard/*.h $(INCDIR)/wiikeyboard

#---------------------------------------------------------------------------------
install: $(PLATFORMS) $(RULES) install-headers
#---------------------------------------------------------------------------------
	@$(eval INSTALL_PREFIX := $(INSTALL_PREFIX)) # Expand
	@mkdir -p $(DESTDIR)$(INSTALL_PREFIX)
	@mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/$(INCDEST)
	@cp -frv include/* -t $(DESTDIR)$(INSTALL_PREFIX)/$(INCDEST)
	@mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/$(LIBDEST)
	@cp -frv lib/* -t $(DESTDIR)$(INSTALL_PREFIX)/$(LIBDEST)
	@cp -frv $(BASEDIR)/*_license.txt $(DESTDIR)$(INSTALL_PREFIX)
	@cp -frv $(RULES) $(DESTDIR)$(INSTALL_PREFIX)

#---------------------------------------------------------------------------------
uninstall:
#---------------------------------------------------------------------------------
	@$(eval INSTALL_PREFIX := $(INSTALL_PREFIX)) # Expand
	@rm -frv $(DESTDIR)$(INSTALL_PREFIX)

#---------------------------------------------------------------------------------
dist: $(PLATFORMS) $(RULES) install-headers
#---------------------------------------------------------------------------------
	@tar -C $(BASEDIR) --exclude-vcs --exclude-vcs-ignores --exclude .github \
		-cvjf $(BUILDDIR)/libogc2-src-$(VERSTRING).tar.bz2 .

	@cp $(BASEDIR)/*_license.txt .
	@tar -cvjf libogc2-$(VERSTRING).tar.bz2 include lib *_license.txt $(RULES)


ifeq ($(strip $(LIBRARIES)),)
LIBRARIES	:=	ogc modplay db tinysmb asnd aesnd iso9660

ifeq ($(PLATFORM),cube)
LIBRARIES	+=	bba
endif
ifeq ($(PLATFORM),wii)
LIBRARIES	+=	bte wiiuse di wiikeyboard
endif
endif

#---------------------------------------------------------------------------------
libs: $(foreach lib,$(LIBRARIES),$(LIBDIR)/lib$(lib).a)
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr wii cube
	rm -fr gc/ogc/libversion.h
	rm -fr $(RULES)
	rm -fr $(DEPS)
	rm -fr $(LIBS)
	rm -fr $(INCDIR)
	rm -f *.map

#---------------------------------------------------------------------------------
docs: install-headers
#---------------------------------------------------------------------------------
	@cd $(BASEDIR); VERSTRING="$(VERSTRING)" doxygen Doxyfile

-include $(DEPSDIR)/*.d
