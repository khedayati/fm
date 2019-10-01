#include "scr.h"
#include "data.h"
#include <errno.h>
#include <signal.h>

#ifndef __STDC_ISO_10646__
    #error "Wide chars are not defined as Unicode codepoints"
#endif

#define pg_up "pg_up"
#define pg_dn "pg_dn"

int maxlen = 40;
volatile sig_atomic_t outside_box = 0;
volatile sig_atomic_t resized = 0;

typedef struct {
    int y_beg;
    int x_beg;
    int y_size;
    int x_size;
    int y_previous;
    int x_previous;
} Window;

typedef struct {
    int array_size;
    int n_to_print;
    int n_lower_t;
    int pos_upper_t;
    int pos_lower_t;
    int option_previous;
} Scroll;

typedef struct {
    int y;
    int x;
} Box;

Window w_main, w1;
Box box;

void highlight(char **entry, int *pos);
int update(Window *w, Scroll *s, int *pos);
void print_debug(Window *w, Scroll *s, int option, int pos, int cursor_pos);
void print_entries(Window *w, Scroll *s, char **entries, int option, int *c, int *pos);
static void sig_win_ch_handler(int sig);
void mvwprintw(Window *win, int y, int x, char *str);
void draw_box(Window *w);
void set_box_size(int y, int x);

int main(void)
{
    setlocale(LC_ALL, "");
    save_config;
    w1.y_beg = 5;
    w1.x_beg = 5;

    char position[strlen(place)];
    if (get_window_size(STDIN_FILENO, STDOUT_FILENO, &w_main.y_size, &w_main.x_size) < 0) {
        restore_config;
        char *error = "Error getting window size";
        sprintf(position, place, w1.y_beg + 1, w1.x_beg + 1);
        move(1, position);
        write(1, error, strlen(error));
        return EXIT_FAILURE;
    }

    int pos = 0;
    w1.y_size = w_main.y_size - w1.y_beg;
    w1.x_size = w_main.x_size - w1.x_beg;
    w1.y_previous = 0;
    w1.x_previous = 0;

    Scroll s;

    struct winsize w_s;
    struct sigaction s_a;
    sigemptyset(&s_a.sa_mask);
    s_a.sa_flags = 0;
    s_a.sa_handler = sig_win_ch_handler;
    char err1[] = "sigaction";
    if (sigaction(SIGWINCH, &s_a, NULL)) {
        restore_config;
        write(STDERR_FILENO, err1, sizeof(err1));
    }

    char space[1] = " ";

    int c = 0,
        debug = 1,
        option = 0,
        i,
        j,
        initial_loop = 1;
    int len = 0;

    for (;;) {
        if (!ioctl(STDIN_FILENO, TIOCGWINSZ, &w_s)) {
            w_main.y_size = w_s.ws_row;
            w_main.x_size = w_s.ws_col;

            w1.y_size = w_main.y_size - 5;
            w1.x_size = w_main.x_size - 2;

            if (w1.y_size > w1.y_previous || w1.x_size > w1.x_previous ||
                w1.y_size < w1.y_previous || w1.x_size < w1.x_previous) {
                erase_scr(STDOUT_FILENO, "\033[2J");

                draw_box(&w1);
                option = update(&w1, &s, &pos);
                if (initial_loop) {
                    for (i = 0; i < s.n_to_print; ++i) {
                        len = strlen(entries[i]);
                        sprintf(position, place, i + w1.y_beg + 1, w1.x_beg + 1);
                        move(1, position);
                        if (i == 0) {
                            write(1, bg_cyan, sizeof(bg_cyan));
                            write(1, entries[i], len);
                            sprintf(position, place, i + w1.y_beg + 1, w1.x_beg + len + 1);
                            move(1, position);
                            for (j = 0; j < maxlen - len; ++j) {
                                write(1, space, sizeof(space));
                            }
                            write(1, bg_reset, sizeof(bg_reset));
                        } else {
                            write(1, entries[i], len);
                        }
                    }
                   initial_loop = 0;
                }
            }
            w1.y_previous = w1.y_size;
            w1.x_previous = w1.x_size;
        }
        print_entries(&w1, &s, entries, option, &c, &pos);
        if (debug) {
            int p = pos - s.pos_upper_t + w1.y_beg + 1;
            print_debug(&w1, &s, option, pos, p);
        }
        if ((c = kbget()) == KEY_ESCAPE) { break; }
    }
    restore_config;
    return 0;
}

void highlight(char **entry, int *pos)
{
    int len = strlen(entry[*pos]);
    char space[1] = " ";
    write(1, bg_cyan, sizeof(bg_cyan));
    write(1, entries[*pos], len);
    int i;
    for (i = 0; i < maxlen - len; ++i) {
        write(1, space, 1);
    }
    write(1, bg_reset, sizeof(bg_reset));
}

static void sig_win_ch_handler(int sig) { resized = 1;}

int update(Window *w, Scroll *s, int *pos)
{
    int option = 0;
    s->array_size = sz;
    int y = w->y_size - w->y_beg - 2;
    if (*pos == 0 ) {
        s->pos_upper_t = 0;
    }
    if (s->array_size <= y) {
        s->n_to_print = s->array_size;
        s->pos_lower_t = s->n_to_print - 1;
        s->n_lower_t = 0;
        if (s->option_previous == 3) {
            s->pos_upper_t = 0;
        }
        if (s->array_size == y) {
            option = 1;
        } else if (s->array_size < y) {
            option = 2;
        }
    } else if (s->array_size > y) {
        s->n_to_print = y;
        if (s->n_to_print < s->pos_upper_t || s->pos_upper_t != 0) {
            s->pos_lower_t = s->n_to_print + s->pos_upper_t - 1;
        } else {
            s->pos_lower_t = s->n_to_print - s->pos_upper_t - 1;
        }
        if (s->pos_lower_t < 0) { s->pos_lower_t = s->array_size - 1; }
        s->n_lower_t = s->array_size - s->pos_lower_t - 1;

        if (s->n_to_print + s->pos_upper_t >= s->array_size) {
            s->n_to_print = s->array_size - s->pos_upper_t;
            s->pos_lower_t = s->array_size - 1;
            s->n_lower_t = s->array_size - s->pos_lower_t - 1;
        }
        if (s->n_to_print != w->y_size - w->y_beg - 2) {
            s->n_to_print = w->y_size - w->y_beg - 2;
            if (s->n_to_print > s->array_size - 1) {
                s->n_to_print = s->array_size - 1;
            }
            s->pos_upper_t = s->pos_lower_t - s->n_to_print;
            s->pos_lower_t = s->pos_upper_t + s->n_to_print - 1;
            s->n_lower_t = s->array_size - s->pos_lower_t - 1;
        }
        if (*pos == s->array_size - 1 && resized) {
            *pos = s->pos_lower_t;
        }
        option = 3;
    }
    s->option_previous = option;
    return option;
}

void print_entries(Window *w, Scroll *s, char **entries, int option, int *c, int *pos)
{
    int i;
    int y = 0;
    int len = 0;
    char pos_c[strlen(place)];
    char in[strlen(del)];
    sprintf(in, del, maxlen);
    char space[1] = " ";

    // rajouter bool_scroll pcq si resize de grand a petit (2 entries) puis regrandit : probleme
    if (s->option_previous != option || resized) {
        int diff = s->pos_lower_t - s->pos_upper_t;
        int j = s->pos_upper_t;
        int k = 0;
        for (i = 0; i <= diff; ++i, ++j) {
            sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
            move(1, pos_c);
            if (j == *pos) {
                sprintf(in, del, maxlen);
                del_from_cursor(in);
                
                len = strlen(entries[j]);
                write(1, bg_cyan, sizeof(bg_cyan));
                write(1, entries[j], len);
                for (k = 0; k < maxlen - len + 1; ++k) {
                    //sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + len + k + 1);
                    //move(1, pos_c);
                    if (k == maxlen - len) {
                        write(1, bg_reset, sizeof(bg_reset));
                    } else {
                        write(1, space, strlen(space));
                    }
                }
                
            } else {
                write(1, entries[j], strlen(entries[j]));
            }
        }
    }

    switch (*c) {
        case KEY_UP:
        case UP:
            if (*pos > 0) {
                --*pos;
                resized = 0;
                if (*pos >= s->pos_upper_t && *pos < s->pos_lower_t) {
                    y = *pos - s->pos_upper_t + w->y_beg + 1;
                    sprintf(pos_c, place, y + 1, w->x_beg + 1);
                    move(1, pos_c);
                    del_from_cursor(in);
                    len = strlen(entries[*pos + 1]);
                    if (*pos + 1 < s->array_size) {
                        write(1, entries[*pos + 1], len);
                    }
                    sprintf(pos_c, place, y, w->x_beg + 1);
                    move(1, pos_c);
                } else if (*pos <= s->pos_upper_t && s->pos_upper_t > 0) {
                    --s->pos_upper_t;
                    ++s->n_lower_t;
                    --s->pos_lower_t;
                    for (i = 0; i < s->n_to_print; ++i) {
                        sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);
                        if (i + w->y_beg < box.y - 3) {
                            sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                            move(1, pos_c);
                        }
                        len = strlen(entries[s->pos_upper_t + i]);
                        if (s->pos_upper_t + i < s->array_size + w->y_beg) {
                            write(1, entries[s->pos_upper_t + i], len);
                        }
                    }
                    if (*pos - s->pos_upper_t + w->y_beg + 1 < w->y_size - 1) {
                        outside_box = 0;
                        del_from_cursor(in);
                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                    } else {
                        outside_box = 1;
                        sprintf(pos_c, place, i + w->y_beg, w->x_beg + 1);
                        move(1, pos_c);
                    }
                }
                if (*pos >= s->pos_lower_t) {
                    *pos = s->pos_lower_t;
                    if (*pos < s->array_size - 1) {
                        sprintf(pos_c, place, *pos + w->y_beg - s->pos_upper_t + 1, w->x_beg + 1);
                        move(1, pos_c);
                    }
                }

                if (*pos < s->pos_lower_t + 1) {
                    del_from_cursor(in);
                    highlight(entries, pos);
                }
            }
            break;
        case KEY_DOWN:
        case DN:
            if (*pos < s->array_size - 1) {
                ++*pos;
                resized = 0;
                if (*pos > s->pos_upper_t && *pos <= s->pos_lower_t) {
                    y = *pos - s->pos_upper_t + w->y_beg;
                    sprintf(pos_c, place, y, w->x_beg + 1);
                    move(1, pos_c);
                    del_from_cursor(in);
                    len = strlen(entries[*pos - 1]);
                    if (*pos - 1 < s->array_size) {
                        write(1, entries[*pos - 1], len);
                    }
                    sprintf(pos_c, place, y + 1, w->x_beg + 1);
                    move(1, pos_c);
                    outside_box = 0;
                } else if (*pos - 1 >= s->pos_lower_t) {
                    ++s->pos_upper_t;
                    --s->n_lower_t;
                    ++s->pos_lower_t;

                    for (i = 0; i < s->n_to_print; ++i) {

                        if (i + w->y_beg + 1 < w->y_size) {
                            sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                            move(1, pos_c);
                        }
                        del_from_cursor(in);

                        if (s->pos_upper_t + i < s->array_size) {
                            write(1, entries[s->pos_upper_t + i], strlen(entries[s->pos_upper_t + i]));
                        }

                    }

                    if (*pos - s->pos_upper_t + w->y_beg + 1 < w->y_size - 1) {
                        outside_box = 0;
                        del_from_cursor(in);
                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                    } else {
                        outside_box = 1;
                        sprintf(pos_c, place, i + w->y_beg, w->x_beg + 1);
                        move(1, pos_c);
                    }
                }
                if (outside_box) {
                    *pos = s->pos_lower_t;
                }
                highlight(entries, pos);
            }
            break;
        case KEY_PAGE_UP:
                //char pg_up[] = "pg_up";
                sprintf(pos_c, place, w->y_size + 2, w->x_beg + 2);
                move(1, pos_c);
                write(1, pg_up, strlen(pg_up));

                if (*pos - s->n_to_print + 1 >= 0) {
                    
                    sprintf(pos_c, place, w->y_size + 3, w->x_beg + 1);
                    move(1, pos_c);
                    del_from_cursor(in);
                    mvwprintw(w, w->y_size + 3, w->x_beg + 1, "1up");


                    if (*pos - s->n_to_print + 1 >= s->pos_upper_t) {

                        sprintf(pos_c, place, w->y_size + 3, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);
                        mvwprintw(w, w->y_size + 3, w->x_beg + 1, "1aup");

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        write(1, entries[*pos], strlen(entries[*pos]));

                        *pos -= (s->n_to_print - 1); // donne *pos = *pos - s->n_to_print + 1
                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);

                        highlight(entries, pos);
                    } else {
                        sprintf(pos_c, place, w->y_size + 3, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);
                        mvwprintw(w, w->y_size + 3, w->x_beg + 1, "1bup");

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        write(1, entries[*pos], strlen(entries[*pos]));
                        if (*pos - s->n_to_print + 1 < s->pos_upper_t) {
                            *pos -= (s->n_to_print - 1);
                            s->pos_upper_t = *pos;
                            s->pos_lower_t = s->pos_upper_t + s->n_to_print - 1;
                            s->n_lower_t = s->array_size - s->pos_lower_t - 1; 
                            
                        }
                        for (i = 0; i < s->n_to_print; ++i) {
                            
                            sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                            move(1, pos_c);
                            del_from_cursor(in);

                            if (*pos + i < s->array_size) {
                                write(1, entries[*pos + i], strlen(entries[*pos + i]));
                            }
                        }

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);

                        highlight(entries, pos);
                    }
                } else if (*pos - s->n_to_print < 0) {
                    sprintf(pos_c, place, w->y_size + 3, w->x_beg + 1);
                    move(1, pos_c);
                    del_from_cursor(in);
                    mvwprintw(w, w->y_size + 3, w->x_beg + 1, "2up");
                    *pos = 0;
                
                    //sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                    //move(1, pos_c);
                    for (i = 0; i < s->n_to_print; ++i) {
                        
                        sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);

                        if (*pos + i < s->array_size) {
                            write(1, entries[*pos + i], strlen(entries[*pos + i]));
                        }
                    }

                    s->pos_lower_t -= s->pos_upper_t;
                    s->pos_upper_t = *pos;
                    s->n_lower_t = s->array_size - s->n_to_print;
                    sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                    move(1, pos_c);

                    highlight(entries, pos);
                }

            break;
        case KEY_PAGE_DN:
                //char pg_dn[] = "pg_dn";
                sprintf(pos_c, place, w->y_size + 2, w->x_beg + 2);
                move(1, pos_c);
                write(1, pg_dn, strlen(pg_dn));

                if (*pos + s->n_to_print - 1 <= s->array_size - 1) {
                    sprintf(pos_c, place, w->y_size + 3, w->x_beg + 1);
                    move(1, pos_c);
                    del_from_cursor(in);
                    mvwprintw(w, w->y_size + 3, w->x_beg + 2, "1");

                    if (*pos + s->n_to_print - 1 <= s->pos_lower_t) { // no scroll

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);

                        sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        write(1, entries[*pos], strlen(entries[*pos]));

                        *pos += (s->n_to_print - 1);
                    } else {
                        del_from_cursor(in);
                        mvwprintw(w, w->y_size + 3, w->x_beg + 1, "1a");
                        s->pos_upper_t = *pos;
                        s->pos_lower_t = *pos + s->n_to_print - 1;
                        s->n_lower_t = s->array_size - s->pos_lower_t - 1;
                        if (s->n_lower_t < 0) {
                            s->n_lower_t = 0;
                        }
                        // va ds 2 ne s'applique pas ici
                        if (s->pos_lower_t > s->array_size - 1) {
                            s->pos_lower_t = s->array_size - 1;
                        }
                        for (i = 0; i < s->n_to_print; ++i) {
                            sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                            move(1, pos_c);
                            del_from_cursor(in);

                            if (*pos + i < s->array_size) {
                                write(1, entries[*pos + i], strlen(entries[*pos + i]));
                                //write(1, "a", strlen("a"));
                            }
                        }
                        *pos += (s->n_to_print - 1);
                    }
                    sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
                    move(1, pos_c);
                    highlight(entries, pos);

                } else if (*pos + s->n_to_print - 1 > s->array_size - 1) {
                    del_from_cursor(in);
                    mvwprintw(w, w->y_size + 3, w->x_beg + 2, "2");

                    for (i = 0; i < s->n_to_print; ++i) {
                        sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                        move(1, pos_c);
                        del_from_cursor(in);

                        write(1, entries[s->pos_upper_t + s->n_lower_t + i], 
                                strlen(entries[s->pos_upper_t + s->n_lower_t + i]));
                    }

                    int prev_i = i;
                    s->pos_upper_t += s->n_lower_t;
                    s->pos_lower_t = s->array_size - 1;
                    s->n_lower_t = 0;

                    *pos = s->array_size - 1;

                    sprintf(pos_c, place, prev_i + w->y_beg, w->x_beg + 1);
                    move(1, pos_c);
                    highlight(entries, pos);
                } else if (*pos + 1 < s->array_size - 1) {
                    del_from_cursor(in);
                    mvwprintw(w, w->y_size + 3, w->x_beg + 2, "3");
                }
            break;
        case KEY_HOME:
            *pos = 0;
            s->pos_upper_t = 0;
            s->pos_lower_t = s->n_to_print - 1;
            s->n_lower_t = s->array_size - s->n_to_print;
            for (i = 0; i < s->n_to_print; ++i) {
                sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                move(1, pos_c);
                del_from_cursor(in);
                write(1, entries[i], strlen(entries[i]));
            }
            sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
            move(1, pos_c);
            highlight(entries, pos);
            break;
        case KEY_END:
            s->pos_upper_t = s->array_size - s->n_to_print;
            *pos = s->array_size - 1;
            s->pos_lower_t = *pos;
            s->n_lower_t = 0;
            for (i = 0; i < s->n_to_print; ++i) {
                sprintf(pos_c, place, i + w->y_beg + 1, w->x_beg + 1);
                move(1, pos_c);
                del_from_cursor(in);
                write(1, entries[i + s->pos_upper_t], strlen(entries[i + s->pos_upper_t]));
            }
            sprintf(pos_c, place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
            move(1, pos_c);
            highlight(entries, pos);
            break;
        default:
            break;
    }

    if (*pos == 0) {
        sprintf(pos_c, place, w->y_beg + 1, w->x_beg + 1);
        move(1, pos_c);
    }

    snprintf(pos_c, strlen(place), place, *pos - s->pos_upper_t + w->y_beg + 1, w->x_beg + 1);
    move(1, pos_c);
}

void draw_box(Window *w)
{
    int horiz = w->y_size - w->y_beg;
    int vert = w->x_size - w->x_beg;
    set_box_size(horiz, vert);
    char *lu_corner = "┌";
    char *ll_corner = "└";
    char *ru_corner = "┐";
    char *rl_corner = "┘";
    char *line = "─";
    char *v_line = "│";
    char position[strlen(place)];
    int i, j;
    for (i = 0; i < horiz; ++i) {                          // to_print + 1
        if (i == 0) {
            sprintf(position, place, w->y_beg, w->x_beg - 1);
            move(1, position);
            for (j = 0; j < vert; ++j) {                 // maxlen + 2
                if (j == 0) {
                    write(1, lu_corner, strlen(lu_corner));
                } else if (j == vert - 1) {
                    write(1, ru_corner, strlen(lu_corner));
                } else {
                    write(1, line, strlen(line));
                }
            }
        }
        if (i < horiz - 1) {
            sprintf(position, place, i + w->y_beg + 1, w->x_beg - 1);
            move(1, position);
            write(1, v_line, strlen(v_line));
            for (j = 0; j < vert - 2; ++j) {
                if (j == vert - 3) {
                    if (i == 0) {
                        sprintf(position, place, i + w->y_beg + 1, w->x_beg + j + 1);
                        move(1, position);
                    } else {
                        sprintf(position, place, i + w->y_beg + 1, w->x_beg + vert - 2);
                        move(1, position);
                    }
                    write(1, v_line, strlen(v_line));
                }
            }
        } else if (i == horiz - 1) {
            for (j = 0; j < vert; ++j) {
                if (j == 0) {
                    sprintf(position, place, i + w->y_beg, w->x_beg - 1);
                    move(1, position);
                    write(1, ll_corner, strlen(ll_corner));
                } else if (j == vert - 1) {
                    sprintf(position, place, i + w->y_beg, w->x_beg + j - 1);
                    move(1, position);
                    write(1, rl_corner, strlen(rl_corner));
                } else {
                    sprintf(position, place, i + w->y_beg, w->x_beg + j - 1);
                    move(1, position);
                    write(1, line, strlen(line));
                }
            }
        }
    }
}

void set_box_size(int y, int x)
{
    box.y = y;
    box.x = x;
}

void mvwprintw(Window *win, int y, int x, char *str)
{
    if (win->y_size > w_main.y_size) {
        char *err = "Error: out of window size: ";
        write(2, err, strlen(err));
        write(2, __func__, strlen(__func__));
    }
    char pos[strlen(place)];
    sprintf(pos, place, y, x);
    move(1, pos);
    write(1, str, strlen(str));
}

void print_debug(Window *w, Scroll *s, int option, int pos, int cursor_pos)
{
    char x_size[3];
    char y_size[2];
    char pos_place[strlen(place)];

    // size of w1 and not of w !
    sprintf(pos_place, place, 1, w->x_size - 3);
    move(1, pos_place);
    sprintf(y_size, "%d", w->y_size);
    write(1, y_size, strlen(y_size));
    //mvwprintw(w, 1, w->x_size - 3, y_size);

    sprintf(pos_place, place, 2, w->x_size - 3);
    move(1, pos_place);
    sprintf(x_size, "%d", w->x_size);
    write(1, x_size, strlen(x_size));
    //mvwprintw(w, 2, w->x_size - 3, x_size);

    char num[18];
    int y_top = w->y_beg - 3;
    int x_top = w->x_beg - 3;
    sprintf(pos_place, place, y_top, x_top);
    move(1, pos_place);
    sprintf(num, "s->n_to_print: %d", s->n_to_print);
    write(1, num, strlen(num));

    sprintf(pos_place, place, w->y_beg + 1, (w->x_size / 2) + 5);
    move(1, pos_place);
    sprintf(num, "s->pos_upper_t: %d", s->pos_upper_t);
    write(1, num, strlen(num));

    //char in[strlen(del)];
    //sprintf(in, del, maxlen);
    //del_from_cursor(in);
    sprintf(pos_place, place, w->y_size - 2, (w->x_size / 2) + 5);
    move(1, pos_place);
    sprintf(num, "s->pos_lower_t: %d", s->pos_lower_t);
    write(1, num, strlen(num));

    sprintf(pos_place, place, w->y_size - 2, (w->x_size / 2) + 25);
    move(1, pos_place);
    sprintf(num, "s->n_lower_t: %d", s->n_lower_t);
    write(1, num, strlen(num));

    char opt[10];
    sprintf(opt, "option: %d", option);
    sprintf(pos_place, place, w->y_size - 3, (w->x_size / 2) + 10);
    move(1, pos_place);
    write(1, opt, strlen(opt));

    sprintf(opt, "pos:    %d", pos);
    sprintf(pos_place, place, w->y_size - 4, (w->x_size / 2) + 10);
    move(1, pos_place);
    write(1, opt, strlen(opt));

    char pos_c[strlen(place)];
    sprintf(pos_c, place, w->y_size + 1, (w->x_size / 2) + 8);
    move(1, pos_c);
    sprintf(num, "box_size_y: %d", box.y);
    write(1, num, strlen(num));

    char y_value[12];
    char x_value[12];
    sprintf(pos_c, place, w->y_size + 4, w->x_size - 20);
    move(1, pos_c);
    char de[2];
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 4, w->x_size - 20);
    move(1, pos_c);
    sprintf(y_value, "y_value: %d", pos - s->pos_upper_t + w->y_beg + 1);
    write(1, y_value, strlen(y_value));

    sprintf(pos_c, place, w->y_size + 3, w->x_size - 20);
    move(1, pos_c);
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 3, w->x_size - 20);
    move(1, pos_c);
    sprintf(x_value, "x_value: %d", w->x_beg + 1);
    write(1, x_value, strlen(x_value));

    char b_value[12];
    sprintf(pos_c, place, w->y_size + 2, w->x_size - 20);
    move(1, pos_c);
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 2, w->x_size - 20);
    move(1, pos_c);
    sprintf(b_value, "b_value: %d", box.y + w->y_beg - 2);
    write(1, b_value, strlen(b_value));

    char w_cur[11];
    sprintf(pos_c, place, w->y_size + 3, w->x_size - 40);
    move(1, pos_c);
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 3, w->x_size - 40);
    move(1, pos_c);
    sprintf(w_cur, "w_cur: %d", w->y_size);
    write(1, w_cur, strlen(w_cur));

    sprintf(pos_c, place, w->y_size + 4, w->x_size - 40);
    move(1, pos_c);
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 4, w->x_size - 40);
    move(1, pos_c);
    sprintf(w_cur, "resized: %d", resized);
    write(1, w_cur, strlen(w_cur));

    sprintf(pos_c, place, w->y_size + 4, w->x_size - 60);
    move(1, pos_c);
    sprintf(de, del, 20);
    del_from_cursor(de);
    sprintf(pos_c, place, w->y_size + 4, w->x_size - 60);
    move(1, pos_c);
    sprintf(w_cur, "outside: %d", outside_box);
    write(1, w_cur, strlen(w_cur));
    // replace le curseur a la 1ere lettre de la dern entree
    sprintf(pos_c, place, cursor_pos, w->x_beg + 1);
    move(1, pos_c);
}
