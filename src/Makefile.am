bin_PROGRAMS = tio
tio_SOURCES = tty.c \
                options.c \
                time.c \
                main.c \
                log.c \
                include/tio/tty.h \
                include/tio/options.h \
                include/tio/time.h \
                include/tio/print.h \
                include/tio/log.h

if ENABLE_BASH_COMPLETION
bashcompletiondir=@BASH_COMPLETION_DIR@
dist_bashcompletion_DATA=bash-completion/tio
endif
