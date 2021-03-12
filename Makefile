# Makefile for building ecierthon
# See ../doc/readme.html for installation and customization instructions.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= guess

CC= gcc -std=gnu99
CFLAGS= -O2 -Wall -Wextra -Decierthon_COMPAT_5_3 $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

AR= ar rcu
RANLIB= ranlib
RM= rm -f
UNAME= uname

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# Special flags for compiler modules; -Os reduces code size.
CMCFLAGS= 

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= guess aix bsd c89 freebsd generic linux linux-readline macosx mingw posix solaris

ecierthon_A=	libecierthon.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

ecierthon_T=	ecierthon
ecierthon_O=	ecierthon.o

ecierthonC_T=	ecierthonc
ecierthonC_O=	ecierthonc.o

ALL_O= $(BASE_O) $(ecierthon_O) $(ecierthonC_O)
ALL_T= $(ecierthon_A) $(ecierthon_T) $(ecierthonC_T)
ALL_A= $(ecierthon_A)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(ecierthon_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(ecierthon_T): $(ecierthon_O) $(ecierthon_A)
	$(CC) -o $@ $(LDFLAGS) $(ecierthon_O) $(ecierthon_A) $(LIBS)

$(ecierthonC_T): $(ecierthonC_O) $(ecierthon_A)
	$(CC) -o $@ $(LDFLAGS) $(ecierthonC_O) $(ecierthon_A) $(LIBS)

test:
	./ecierthon -v

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(SYSLDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"
	@echo "UNAME= $(UNAME)"

# Convenience targets for popular platforms.
ALL= all

help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See doc/readme.html for complete instructions."

guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

AIX aix:
	$(MAKE) $(ALL) CC="xlc" CFLAGS="-O2 -Decierthon_USE_POSIX -Decierthon_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_POSIX -Decierthon_USE_DLOPEN" SYSLIBS="-Wl,-E"

c89:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_C89" CC="gcc -std=c89"
	@echo ''
	@echo '*** C89 does not guarantee 64-bit integers for ecierthon.'
	@echo ''

FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_LINUX -Decierthon_USE_READLINE -I/usr/include/edit" SYSLIBS="-Wl,-E -ledit" CC="cc"

generic: $(ALL)

Linux linux:	linux-noreadline

linux-noreadline:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_LINUX" SYSLIBS="-Wl,-E -ldl"

linux-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_LINUX -Decierthon_USE_READLINE" SYSLIBS="-Wl,-E -ldl -lreadline"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_MACOSX -Decierthon_USE_READLINE" SYSLIBS="-lreadline"

mingw:
	$(MAKE) "ecierthon_A=ecierthon54.dll" "ecierthon_T=ecierthon.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-Decierthon_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" ecierthon.exe
	$(MAKE) "ecierthonC_T=ecierthonc.exe" ecierthonc.exe

posix:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_POSIX"

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-Decierthon_USE_POSIX -Decierthon_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help test clean default o a depend echo

# Compiler modules may use special flags.
llex.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c llex.c

lparser.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lparser.c

lcode.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lcode.c

# DO NOT DELETE

lapi.o: lapi.c lprefix.h ecierthon.h ecierthonconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h \
 ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h
lbaselib.o: lbaselib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lcode.o: lcode.c lprefix.h ecierthon.h ecierthonconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lgc.h lstring.h ltable.h lvm.h
lcorolib.o: lcorolib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lctype.o: lctype.c lprefix.h lctype.h ecierthon.h ecierthonconf.h llimits.h
ldblib.o: ldblib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
ldebug.o: ldebug.c lprefix.h ecierthon.h ecierthonconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h \
 ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c lprefix.h ecierthon.h ecierthonconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lparser.h lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.c lprefix.h ecierthon.h ecierthonconf.h lobject.h llimits.h lstate.h \
 ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h
lgc.o: lgc.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lprefix.h ecierthon.h ecierthonconf.h ecierthonlib.h lauxlib.h
liolib.o: liolib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
llex.o: llex.c lprefix.h ecierthon.h ecierthonconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h \
 lstring.h ltable.h
lmathlib.o: lmathlib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lmem.o: lmem.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
loadlib.o: loadlib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lobject.o: lobject.c lprefix.h ecierthon.h ecierthonconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h \
 lvm.h
lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h ecierthon.h ecierthonconf.h
loslib.o: loslib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lparser.o: lparser.c lprefix.h ecierthon.h ecierthonconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lprefix.h ecierthon.h ecierthonconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
 lstring.h ltable.h
lstring.o: lstring.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
ltable.o: ltable.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ltablib.o: ltablib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
ltm.o: ltm.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ecierthon.o: ecierthon.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
ecierthonc.o: ecierthonc.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h lopcodes.h lopnames.h lundump.h
lundump.o: lundump.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h \
 lundump.h
lutf8lib.o: lutf8lib.c lprefix.h ecierthon.h ecierthonconf.h lauxlib.h ecierthonlib.h
lvm.o: lvm.c lprefix.h ecierthon.h ecierthonconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h \
 ltable.h lvm.h ljumptab.h
lzio.o: lzio.c lprefix.h ecierthon.h ecierthonconf.h llimits.h lmem.h lstate.h \
 lobject.h ltm.h lzio.h

# (end of Makefile)
