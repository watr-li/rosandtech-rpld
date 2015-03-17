#
# RPL Linux implementation
#        Makefile
#

PROJECT = rpld
VERSION = 1.00

UIP_CONF_IPV6     = 1
UIP_CONF_IPV6_RPL = 1

CONTIKI_TARGET_MAIN = main.o

CONTIKI_TARGET_SOURCEFILES += rpld.c table.c prefix.c redirect.c

CONTIKI = contiki

include $(CONTIKI)/Makefile.include

CLEAN += $(PROJECT) $(PROJECT).$(TARGET) contiki-$(TARGET).a

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL} -m 755
INSTALL_SCRIPT = ${INSTALL} -m 755

all: $(PROJECT)
	cp $(PROJECT).$(TARGET) $<

install:
	mkdir -p ${prefix}/bin
	${INSTALL_PROGRAM} $(PROJECT) ${prefix}/bin
ifneq ($(mandir),)
	${INSTALL_DATA} $(PROJECT).8 ${mandir}/man8
endif

