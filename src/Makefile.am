AM_CFLAGS = -Wall
bin_PROGRAMS = tio
tio_SOURCES = tty.c \
              options.c \
              time.c \
              main.c \
              log.c \
              error.c \
              include/tio/tty.h \
              include/tio/options.h \
              include/tio/time.h \
              include/tio/print.h \
              include/tio/log.h \
              include/tio/error.h

if ENABLE_BASH_COMPLETION
CLEANFILES = bash-completion/tio

do_subst = sed -e 's/57600 /$(B57600) /g' \
               -e 's/115200 /$(B115200) /g' \
               -e 's/230400 /$(B230400) /g' \
               -e 's/460800 /$(B460800) /g' \
               -e 's/500000 /$(B500000) /g' \
               -e 's/576000 /$(B576000) /g' \
               -e 's/921600 /$(B921600) /g' \
               -e 's/1000000 /$(B1000000) /g' \
               -e 's/1152000 /$(B1152000) /g' \
               -e 's/1500000 /$(B1500000) /g' \
               -e 's/2000000 /$(B2000000) /g' \
               -e 's/2500000 /$(B2500000) /g' \
               -e 's/3000000 /$(B3000000) /g' \
               -e 's/3500000 /$(B3500000) /g' \
               -e 's/4000000/$(B4000000)/g'

bash-completion/tio: bash-completion/tio.in
	$(do_subst) < bash-completion/tio.in > bash-completion/tio

bashcompletiondir=@BASH_COMPLETION_DIR@
dist_bashcompletion_DATA=bash-completion/tio

endif
