bin_PROGRAMS = gotty
gotty_SOURCES = tty.c \
                options.c \
                time.c \
                main.c \
                log.c \
                include/gotty/tty.h \
                include/gotty/options.h \
                include/gotty/time.h \
                include/gotty/print.h \
                include/gotty/log.h

if ENABLE_BASH_COMPLETION
bashcompletiondir=@BASH_COMPLETION_DIR@
dist_bashcompletion_DATA=bash-completion/gotty
endif
