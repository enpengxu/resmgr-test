ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

# CFLAGS += "-g -O0 -ggdb3"
# LDFLAGS += -L/home/leoxu/svn/700/mainline/mainline/lib/c/lib/x86_64/a -Wl,-Bstatic -l:libc.a -Wl,-Bdynamic

include $(MKFILES_ROOT)/qtargets.mk

