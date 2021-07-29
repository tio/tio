#include "tio/lineedit.h"
#include "tio/options.h"
#include "tio/print.h"

static struct winsize winsize;

static char edited_line[LINE_SIZE];
static unsigned char edited_line_ctr = 0;
static int edited_line_cursor_pos = 0;
static const char * line_edit_prefix;
static int line_edit_prefix_length;
static char prev_char = 0, prevprev_char = 0;

static char ** history = NULL;
static unsigned int history_max_len;
static unsigned int history_len = 0;
static unsigned int history_pointer = 0;

static void free_history(void){
    for(unsigned int i = 0; i < history_len; i++){
        if(history[i]){
            free(history[i]);
        }
    }

    free(history);
}

static char * get_from_history(int dir){
    char * ret = NULL;
    
    if(dir == 1){
        ret = history[history_pointer];

        if(history_pointer + 1 < history_len){
            history_pointer++;
        }
    } else if(dir == -1){ 
        if(history_pointer > 0){
            history_pointer--;
        }

        ret = history[history_pointer];
    }

    return ret;
}

void lineedit_configure(const char * prefix, size_t max_len){
    if(ioctl(0, TIOCGWINSZ, &winsize) < 0){
        warning_printf("Could not get the dimensions of the terminal");
        exit(EXIT_FAILURE);
    }

    history_max_len = max_len;
    history = (char **) calloc(history_max_len, sizeof(char *));    
    if(history == NULL){
        warning_printf("Could not allocate the history");
        exit(EXIT_FAILURE);
    }

    atexit(free_history);

    line_edit_prefix = prefix;
    line_edit_prefix_length = strlen(prefix);
}

char * get_line(char c){
    char * finished_line = NULL;
    char * history_line = NULL;
    
    if(prev_char == ACTION_PREFIX_1 && c == ACTION_PREFIX_2){
        /* do nothing */
    } else if(prevprev_char == ACTION_PREFIX_1 && prev_char == ACTION_PREFIX_2){
        switch(c){
        case ARROW_LEFT:
            if(edited_line_cursor_pos > 0){
                edited_line_cursor_pos--;
            }
            break;

        case ARROW_RIGHT:
            if(edited_line_cursor_pos < edited_line_ctr){
                edited_line_cursor_pos++;
            }

            break;

        case ARROW_UP:
            if((history_line = get_from_history(1)) != NULL){
                strcpy(edited_line, history_line);
                edited_line_ctr = strlen(history_line);
                edited_line_cursor_pos = edited_line_ctr;
            }
            break;
    
        case ARROW_DOWN:
            if((history_line = get_from_history(-1)) != NULL){
                strcpy(edited_line, history_line);
                edited_line_ctr = strlen(history_line);
                edited_line_cursor_pos = edited_line_ctr;
            }
            break;
        }
    } else {
         if(c == '\r' || c == '\n'){
            edited_line[edited_line_ctr] = '\0';
            finished_line = strdup(edited_line);
        } else {
            if(isprint(c)){
            	if(edited_line_ctr < LINE_SIZE - 1){
            		memmove(&edited_line[edited_line_cursor_pos + 1], 
            			&edited_line[edited_line_cursor_pos], 
            			edited_line_ctr - edited_line_cursor_pos);
            	
                	edited_line[edited_line_cursor_pos] = c;
                
                	edited_line_ctr++;
                	edited_line_cursor_pos++;
            	}
            } else if(c == BACKSPACE){
                char * rest = &edited_line[edited_line_cursor_pos];
                memcpy(&edited_line[edited_line_cursor_pos - 1], rest, edited_line_ctr - edited_line_cursor_pos);

                if(edited_line_ctr > 0){
                    edited_line_ctr--;
                }

                if(edited_line_cursor_pos > 0){
                    edited_line_cursor_pos--;
                }
            }
        }   
    }

	/* clear line with spaces */
    fprintf(stdout, "\r%*c", winsize.ws_col, ' ');
    /* output the contents of the buffer */
    fprintf(stdout, "\r%s%.*s", line_edit_prefix, edited_line_ctr, edited_line);
    
    /* set the position of the cursor */
    GOTOXY(edited_line_cursor_pos + line_edit_prefix_length + 1, winsize.ws_row);

    fflush(stdout);

    /* clear buffer */
    if(finished_line != NULL){
        memset(edited_line, 0, LINE_SIZE);
        edited_line_ctr = 0;
        edited_line_cursor_pos = 0;
    }

    prevprev_char = prev_char;
    prev_char = c;

    return finished_line;
}

void free_line(char * line){
    free(line);
    fprintf(stdout, "\r%s", line_edit_prefix);
}

void add_to_history(const char * line){    
    /* dont add duplicate duplicate */
    if(history_len > 0 && strcmp(line, history[0]) == 0){
        return;
    }
    
    /* history is full, make room */
    if(history_len == history_max_len){
        free(history[history_len - 1]);
    }

    /* shift history to the right by 1 */
    size_t count = history_len == history_max_len ? history_len - 1 : history_len;
    memmove(&history[1], &history[0], count * sizeof(char *));

    /* add to history */
    history[0] = strdup(line);
    if(history[0] == NULL){
        return;
    }

    if(history_len < history_max_len){
        history_len++;
    }

    history_pointer = 0;
}


