/* GnollHack File Change Notice: This file has been changed from the original. Date of last change: 2023-08-07 */

/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* GnollHack 4.0 cursmesg.c */
/* Copyright (c) Karl Garrison, 2010. */
/* GnollHack may be freely redistributed.  See license for details. */

#include "curses.h"
#include "hack.h"
#include "wincurs.h"
#include "cursmesg.h"
#include <ctype.h>

/*
 * Note: references to "More>>" mean ">>", the curses rendition of "--More--".
 */

/* player can type ESC at More>> prompt to avoid seeing more messages
   for the current move; but hero might get more than one move per turn,
   so the input routines need to be able to cancel this */
long curs_mesg_suppress_turn = -1;

/* Message window routines for curses interface */

/* Private declatations */

typedef struct nhpm {
    char *str;                  /* Message text */
    char* attrs;                /* Message attributes */
    char* colors;               /* Message colors */
    long turn;                  /* Turn number for message */
    struct nhpm *prev_mesg;     /* Pointer to previous message */
    struct nhpm *next_mesg;     /* Pointer to next message */
} nhprev_mesg;

static void scroll_window(winid wid);
static void unscroll_window(winid wid);
static void directional_scroll(winid wid, int nlines);
static void mesg_add_line(const char* mline);
static void mesg_add_line_ex(const char *mline, const char* attrs, const char* colors, int attr, int color);
static nhprev_mesg *get_msg_line(boolean reverse, int mindex);

static int turn_lines = 0;
static int mx = 0;
static int my = 0;              /* message window text location */
static nhprev_mesg *first_mesg = NULL;
static nhprev_mesg *last_mesg = NULL;
static int max_messages;
static int num_messages = 0;
static int last_messages = 0;

/* Write string to the message window.  Attributes set by calling function. */

void
curses_message_win_puts(const char* message, boolean recursed)
{
    curses_message_win_puts_ex(message, (char*)0, (char*)0, ATR_NONE, NO_COLOR, recursed);
}

void
curses_message_win_puts_ex(const char *message, const char* attrs, const char* colors, int attr, int color, boolean recursed)
{
    if (!message)
        return;

    int height, width, border_space, linespace;
    char *tmpstr;
    WINDOW *win = curses_get_nhwin(MESSAGE_WIN);
    boolean bold, border = curses_window_has_border(MESSAGE_WIN);
    int message_length = (int) strlen(message);

#if 0
    /*
     * This was useful when curses used genl_putmsghistory() but is not
     * needed now that it has its own curses_putmsghistory() which is
     * capable of putting something into the ^P recall history without
     * displaying it at the same time.
     */
    if (strncmp("Count:", message, 6) == 0) {
        curses_count_window(message);
        return;
    }
#endif

    if (curs_mesg_suppress_turn == moves) {
        return; /* user has typed ESC to avoid seeing remaining messages. */
    }

    curses_get_window_size(MESSAGE_WIN, &height, &width);
    border_space = (border ? 1 : 0);
    if (mx < border_space)
        mx = border_space;
    if (my < border_space)
        my = border_space;

    if (strcmp(message, "#") == 0) {    /* Extended command or Count: */
        if ((strcmp(toplines, "#") != 0)
            /* Bottom of message window */
            && (my >= (height - 1 + border_space)) && (height != 1)) {
            scroll_window(MESSAGE_WIN);
            mx = width;
            my--;
            strcpy(toplines, message);
        }
        return;
    }

    if (!recursed) 
    {
        strcpy(toplines, message);
        size_t len = strlen(message);
        if(attrs)
            memcpy(toplineattrs, attrs, len);
        else
            memset(toplineattrs, attr, len);

        if (colors)
            memcpy(toplinecolors, colors, len);
        else
            memset(toplinecolors, color, len);

        toplineattrs[len] = toplinecolors[len] = 0;

        mesg_add_line_ex(message, attrs, colors, attr, color);
    }

    linespace = width - 3 - (mx - border_space);

    if (linespace < message_length) 
    {
        if (my - border_space >= height - 1) 
        {
            /* bottom of message win */
            if (++turn_lines >= height) 
            { /* || height == 1) */
                /* Pause until key is hit - Esc suppresses any further
                   messages that turn */
                if (curses_more() == '\033') 
                {
                    curs_mesg_suppress_turn = moves;
                    return;
                }
                /* turn_lines reset to 0 by more()->block()->got_input() */
            } 
            else 
            {
                scroll_window(MESSAGE_WIN);
            }
        }
        else
        {
            if (mx != border_space) 
            {
                my++;
                mx = border_space;
                ++turn_lines;
            }
        }
    }

    bold = (height > 1 && !last_messages);
    //if (bold)
    //    curses_toggle_color_attr(win, NONE, A_BOLD, ON);

    /* will this message fit as-is or do we need to split it? */
    if (mx == border_space && message_length > width - 2) 
    {
        /* split needed */
        tmpstr = curses_break_str(message, (width - 2), 1);
        curses_print_text_ex(win, &mx, &my, tmpstr, attrs, colors, attr, color, bold ? ATR_BOLD : ATR_NONE);
        //mvwprintw(win, my, mx, "%s", tmpstr);
        //mx += (int)strlen(tmpstr);
        /* one space to separate first part of message from rest [is this
           actually useful?] */
        if (mx < width - 2)
            ++mx;
        free(tmpstr);
        //if (bold)
        //    curses_toggle_color_attr(win, NONE, A_BOLD, OFF);
        tmpstr = curses_str_remainder(message, (width - 2), 1);
        const char* remainder_attrs = attrs;
        const char* remainder_colors = colors;
        if (tmpstr && (remainder_attrs || remainder_colors))
        {
            char* p = strstr(message, tmpstr);
            if (p)
            {
                int offset = (int)(p - message);
                int rlen = (int)strlen(tmpstr);
                if (rlen > 0 && offset > 0 && rlen + offset <= message_length)
                {
                    if(remainder_attrs)
                        remainder_attrs += offset;
                    if (remainder_colors)
                        remainder_colors += offset;
                }
            }
        }
        curses_message_win_puts_ex(tmpstr, remainder_attrs, remainder_colors, attr, color, TRUE);
        free(tmpstr);
    } 
    else 
    {
        //mvwprintw(win, my, mx, "%s", message);
        //mx += message_length;
        curses_print_text_ex(win, &mx, &my, message, attrs, colors, attr, color, bold ? ATR_BOLD : ATR_NONE);
        /* two spaces to separate this message from next one if they happen
           to fit on the same line; (FIXME:  it would be better if this was
           done at start of next message rather than end of this one since
           it impacts placement of "More>>") */
        if (mx < width - 2) 
        {
            if (++mx < width - 2)
                ++mx;
        }
        //if (bold)
        //    curses_toggle_color_attr(win, NONE, A_BOLD, OFF);
    }
    wrefresh(win);
}

void
curses_got_input(void)
{
    /* if messages are being suppressed, reenable them */
    curs_mesg_suppress_turn = -1;

    /* misleadingly named; represents number of lines delivered since
       player was sure to have had a chance to read them; if player
       has just given input then there aren't any such lines right;
       that includes responding to More>> even though it stays same turn */
    turn_lines = 0;
}

int
curses_block(boolean noscroll) /* noscroll - blocking because of msgtype
                                * = stop/alert else blocking because
                                * window is full, so need to scroll after */
{
    static const char resp[] = " \r\n\033"; /* space, enter, esc */
    static int prev_x = -1, prev_y = -1, blink = 0;
    int height, width, moreattr, oldcrsr, ret = 0,
        brdroffset = curses_window_has_border(MESSAGE_WIN) ? 1 : 0;
    WINDOW *win = curses_get_nhwin(MESSAGE_WIN);

    curses_get_window_size(MESSAGE_WIN, &height, &width);
    if (mx - brdroffset > width - 3) { /* -3: room for ">>_" */
        if (my - brdroffset < height - 1)
            ++my, mx = brdroffset;
        else
            mx = width - 3 + brdroffset;
    }
    /* if ">>" (--More--) is being rendered at the same spot as before,
       toggle attributes so that the first '>' starts blinking if it wasn't
       or stops blinking if it was */
    if (mx == prev_x && my == prev_y) {
        blink = 1 - blink;
    } else {
        prev_x = mx, prev_y = my;
        blink = 0;
    }
    moreattr = !iflags.wc2_guicolor ? (int)A_REVERSE : NONE;
    curses_toggle_color_attr(win, MORECOLOR, moreattr, ON);
    if (blink) {
        wattron(win, A_BLINK);
        mvwprintw(win, my, mx, ">"), mx += 1;
        wattroff(win, A_BLINK);
        waddstr(win, ">"), mx += 1;
    } else {
        mvwprintw(win, my, mx, ">>"), mx += 2;
    }
    curses_toggle_color_attr(win, MORECOLOR, moreattr, OFF);
    wrefresh(win);

    /* cancel mesg suppression; all messages will have had chance to be read */
    curses_got_input();

    oldcrsr = curs_set(1);
    do {
        ret = wgetch(win);
        if (ret == ERR || ret == '\0')
            ret = '\n';
        /* msgtype=stop should require space/enter rather than any key,
           as we want to prevent YASD from direction keys. */
    } while (!index(resp, (char) ret));
    if (oldcrsr >= 0)
        (void) curs_set(oldcrsr);

    if (height == 1) {
        curses_clear_unhighlight_message_window();
    } else {
        mx -= 2, mvwprintw(win, my, mx, "  "); /* back up and blank out ">>" */
        if (!noscroll) {
            scroll_window(MESSAGE_WIN);
        }
        wrefresh(win);
    }
    return ret;
}

int
curses_more()
{
    return curses_block(FALSE);
}


/* Clear the message window if one line; otherwise unhighlight old messages */
void
curses_clear_unhighlight_message_window()
{
    int mh, mw, count,
        brdroffset = curses_window_has_border(MESSAGE_WIN) ? 1 : 0;
    WINDOW *win = curses_get_nhwin(MESSAGE_WIN);

    turn_lines = 0;
    curses_get_window_size(MESSAGE_WIN, &mh, &mw);

    if (mh == 1) 
    {
        curses_clear_nhwin(MESSAGE_WIN);
        mx = my = brdroffset;
    }
    else 
    {
        mx = mw + brdroffset; /* Force new line on new turn */
        for (count = 0; count < mh; count++)
        {
             //mvwchgat(win, count + brdroffset, brdroffset,
             //         mw, COLOR_PAIR(8), A_NORMAL, NULL);
            int x;
            for (x = 0; x < mw; x++)
            {
                chtype ch = mvwinch(win, count + brdroffset, brdroffset + x);
                if (ch & A_BOLD) /* has highlight */
                {
                    ch &= ~A_BOLD; /* Remove highlight */
                    waddch(win, ch);
                }
            }
        }
        wnoutrefresh(win);
    }
    wmove(win, my, mx);
}


/* Reset message window cursor to starting position, and display most
   recent messages. */

void
curses_last_messages()
{
    boolean border = curses_window_has_border(MESSAGE_WIN);
    nhprev_mesg *mesg;
    int i, j, height, width;

    curses_get_window_size(MESSAGE_WIN, &height, &width);

    if (border)
        mx = my = 1;
    else
        mx = my = 0;

    ++last_messages;
    for (j = 0, i = num_messages - 1; i > 0 && j < height; --i, ++j) {
        mesg = get_msg_line(TRUE, i);
        if (mesg && mesg->str && *mesg->str)
            curses_message_win_puts_ex(mesg->str, mesg->attrs, mesg->colors, ATR_NONE, NO_COLOR, TRUE);
    }
    curses_message_win_puts_ex(toplines, toplineattrs, toplinecolors, ATR_NONE, NO_COLOR, TRUE);
    --last_messages;
}


/* Initialize list for message history */

void
curses_init_mesg_history()
{
    max_messages = iflags.msg_history;

    if (max_messages < 1) {
        max_messages = 1;
    }

    if (max_messages > MESG_HISTORY_MAX) {
        max_messages = MESG_HISTORY_MAX;
    }
}

/* Delete message history at game end. */

void
curses_teardown_messages(void)
{
    nhprev_mesg *current_mesg;

    while ((current_mesg = first_mesg) != 0) {
        first_mesg = current_mesg->next_mesg;
        free(current_mesg->str);
        free(current_mesg->attrs);
        free(current_mesg->colors);
        free(current_mesg);
    }
    last_mesg = (nhprev_mesg *) 0;
    num_messages = 0;
}

/* Display previous message window messages in reverse chron order */

void
curses_prev_mesg()
{
    int count;
    winid wid;
    long turn = 0;
    anything Id;
    nhprev_mesg *mesg;
    menu_item *selected = NULL;
    boolean do_lifo = (iflags.prevmsg_window != 'f');

    wid = curses_get_wid(NHW_MENU);
    curses_create_nhmenu(wid, GHMENU_STYLE_PREVIOUS_MESSAGES);
    Id = zeroany;

    for (count = 0; count < num_messages; ++count) {
        mesg = get_msg_line(do_lifo, count);
        if (turn != mesg->turn && count != 0) {
            curses_add_menu(wid, NO_GLYPH, &Id, 0, 0, ATR_NONE, NO_COLOR, "---", FALSE);
        }
        curses_add_menu_ex(wid, NO_GLYPH, &Id, 0, 0, mesg->attrs ? mesg->attrs[0] : ATR_NONE, mesg->colors ? mesg->colors[0] : NO_COLOR, mesg->str, mesg->attrs, mesg->colors, FALSE);
        turn = mesg->turn;
    }
    if (!count)
        curses_add_menu(wid, NO_GLYPH, &Id, 0, 0, ATR_NONE, NO_COLOR,
                        "[No past messages available.]", FALSE);

    curses_end_menu_ex(wid, "", (const char*)0);
    if (!do_lifo)
        curs_menu_set_bottom_heavy(wid);
    curses_select_menu(wid, PICK_NONE, &selected);
    curses_del_wid(wid);
}


/* Display at the bottom of the message window without remembering the
   line for ^P recall.  Used for putstr(WIN_MESSAGE,ATR_NOHISTORY,text)
   which core currently uses for 'Count: 123' and dolook's autodescribe.
   popup_dialog is not currently implemented for this function. */

void
curses_count_window(const char *count_text)
{
    static WINDOW *countwin = NULL;
    int startx, starty, winx, winy;
    int messageh, messagew;

    if (!count_text) {
        if (countwin)
            delwin(countwin), countwin = NULL;
        counting = FALSE;
        return;
    }
    counting = TRUE;

    curses_get_window_xy(MESSAGE_WIN, &winx, &winy);
    curses_get_window_size(MESSAGE_WIN, &messageh, &messagew);

    if (curses_window_has_border(MESSAGE_WIN)) {
        winx++;
        winy++;
    }

    winy += messageh - 1;

#ifdef PDCURSES
    if (countwin)
        curses_destroy_win(countwin), countwin = NULL;
#endif /* PDCURSES */
    /* this used to specify a width of 25; that was adequate for 'Count: 123'
       but not for dolook's autodescribe when it refers to a named monster */
    if (!countwin)
        countwin = newwin(1, messagew, winy, winx);
    startx = 0;
    starty = 0;

    mvwprintw(countwin, starty, startx, "%-*s", messagew, count_text);
    wrefresh(countwin);
}

/* Gets a "line" (buffer) of input. */
void
curses_message_win_getline(int attr, int color, const char *prompt, char *answer, int buffer)
{
    int height, width; /* of window */
    char *tmpbuf, *p_answer; /* combined prompt + answer */
    int nlines, maxlines, i; /* prompt + answer */
    int promptline;
    int promptx;
    char **linestarts; /* pointers to start of each line */
    char *tmpstr; /* for free() */
    int maxy, maxx; /* linewrap / scroll */
    int ch;
    WINDOW *win = curses_get_nhwin(MESSAGE_WIN);
    int border_space = 0;
    int len = 0; /* of answer string */
    boolean border = curses_window_has_border(MESSAGE_WIN);
    int curses_attr = curses_atr2cursesattr(attr);

    *answer = '\0';
    orig_cursor = curs_set(0);

    curses_get_window_size(MESSAGE_WIN, &height, &width);
    if (border) {
        height -= 2, width -= 2;
        border_space = 1;
        if (mx < 1)
            mx = 1;
        if (my < 1)
            my = 1;
    }
    maxy = height - 1 + border_space;
    maxx = width - 1 + border_space;

    tmpbuf = (char *) alloc((unsigned) ((int) strlen(prompt) + buffer + 2));
    maxlines = buffer / width * 2;
    Strcpy(tmpbuf, prompt);
    Strcat(tmpbuf, " ");
    nlines = curses_num_lines(tmpbuf, width);
    maxlines += nlines * 2;
    linestarts = (char **) alloc((unsigned) (maxlines * sizeof (char *)));
    p_answer = tmpbuf + strlen(tmpbuf);
    linestarts[0] = tmpbuf;

    if (mx > border_space) { /* newline */
        if (my >= maxy)
            scroll_window(MESSAGE_WIN);
        else
            my++;
        mx = border_space;
    }

    curses_toggle_color_attr(win, color == NO_COLOR ? NONE : color, curses_attr | A_BOLD, ON);

    for (i = 0; i < nlines - 1; i++) {
        tmpstr = curses_break_str(linestarts[i], width - 1, 1);
        linestarts[i + 1] = linestarts[i] + (int) strlen(tmpstr);
        if (*linestarts[i + 1] == ' ')
            linestarts[i + 1]++;
        mvwaddstr(win, my, mx, tmpstr);
        free(tmpstr);
        if (++my >= maxy) {
            scroll_window(MESSAGE_WIN);
            my--;
        }
    }
    mvwaddstr(win, my, mx, linestarts[nlines - 1]);
    mx = promptx = (int) strlen(linestarts[nlines - 1]) + border_space;
    promptline = nlines - 1;

    while (1) {
        mx = (int) strlen(linestarts[nlines - 1]) + border_space;
        if (mx > maxx) {
            if (nlines < maxlines) {
                tmpstr = curses_break_str(linestarts[nlines - 1],
                                          width - 1, 1);
                mx = (int) strlen(tmpstr) + border_space;
                mvwprintw(win, my, mx, "%*c", maxx - mx + 1, ' ');
                if (++my > maxy) {
                    scroll_window(MESSAGE_WIN);
                    my--;
                }
                mx = border_space;
                linestarts[nlines] = linestarts[nlines - 1]
                                    + (int) strlen(tmpstr);
                if (*linestarts[nlines] == ' ')
                    linestarts[nlines]++;
                mvwaddstr(win, my, mx, linestarts[nlines]);
                mx = (int) strlen(linestarts[nlines]) + border_space;
                nlines++;
                free(tmpstr);
            } else {
                p_answer[--len] = '\0';
                mvwaddch(win, my, --mx, ' ');
            }
        }
        wmove(win, my, mx);
        curs_set(1);
        wrefresh(win);
        curses_got_input(); /* despite its name, before rathre than after... */
#ifdef PDCURSES
        ch = wgetch(win);
#else
        ch = getch();
#endif
#if 0   /* [erase_char (delete one character) and kill_char (delete all
         * characters) are from tty and not currently set up for curses] */
        if (ch == erase_char) {
            ch = '\177'; /* match switch-case below */

        /* honor kill_char if it's ^U or similar, but not if it's '@' */
        } else if (ch == kill_char && (ch < ' ' || ch >= '\177')) { /*ASCII*/
            if (len == 0) /* nothing to kill; just start over */
                continue;
            ch = '\033'; /* get rid of all current input, then start over */
        }
#endif
        curs_set(0);
        switch (ch) {
        case '\033': /* DOESCAPE */
            /* if there isn't any input yet, return ESC */
            if (len == 0) {
                Strcpy(answer, "\033");
                return;
            }
            /* otherwise, discard current input and start over;
               first need to blank it from the screen */
            while (nlines - 1 > promptline) {
                if (nlines-- > height) {
                    unscroll_window(MESSAGE_WIN);
                    tmpstr = curses_break_str(linestarts[nlines - height],
                                              width - 1, 1);
                    mvwaddstr(win, border_space, border_space, tmpstr);
                    free(tmpstr);
                } else {
                    mx = border_space;
                    mvwprintw(win, my, mx, "%*c", maxx - mx, ' ');
                    my--;
                }
            }
            mx = promptx;
            mvwprintw(win, my, mx, "%*c", maxx - mx, ' ');
            *p_answer = '\0';
            len = 0;
            break;
        case ERR: /* should not happen */
            *answer = '\0';
            free(tmpbuf);
            free(linestarts);
            curs_set(orig_cursor);
            curses_toggle_color_attr(win, color == NO_COLOR ? NONE : color, curses_attr | A_BOLD, OFF);
            return;
        case '\r':
        case '\n':
            free(linestarts);
            Strncpy(answer, p_answer, buffer);
            answer[buffer - 1] = '\0';
            Strcpy(toplines, tmpbuf);
            mesg_add_line(tmpbuf);
            free(tmpbuf);
            curs_set(orig_cursor);
            curses_toggle_color_attr(win, color == NO_COLOR ? NONE : color, curses_attr | A_BOLD, OFF);
            if (++my > maxy) {
                scroll_window(MESSAGE_WIN);
                my--;
            }
            mx = border_space;
            return;
        case '\177': /* DEL/Rubout */
        case KEY_DC: /* delete-character */
        case '\b': /* ^H (Backspace: '\011') */
        case KEY_BACKSPACE:
            if (len < 1) {
                len = 1;
                mx = promptx;
            }
            p_answer[--len] = '\0';
            mvwaddch(win, my, --mx, ' ');
            /* try to unwrap back to the previous line if there is one */
            if (nlines > 1 && (int) strlen(linestarts[nlines - 2]) < width) {
                mvwaddstr(win, my - 1, border_space, linestarts[nlines - 2]);
                if (nlines-- > height) {
                    unscroll_window(MESSAGE_WIN);
                    tmpstr = curses_break_str(linestarts[nlines - height],
                                              width - 1, 1);
                    mvwaddstr(win, border_space, border_space, tmpstr);
                    free(tmpstr);
                } else {
                    /* clean up the leftovers on the next line,
                       if we didn't scroll it away */
                    mvwprintw(win, my--, border_space, "%*c",
                              (int) strlen(linestarts[nlines]), ' ');
                }
            }
            break;
        default:
            p_answer[len++] = ch;
            if (len >= buffer)
                len = buffer - 1;
            else
                mvwaddch(win, my, mx, ch);
            p_answer[len] = '\0';
        }
    }
}

/* Scroll lines upward in given window, or clear window if only one line. */
static void
scroll_window(winid wid)
{
    directional_scroll(wid, 1);
}

static void
unscroll_window(winid wid)
{
    directional_scroll(wid, -1);
}

static void
directional_scroll(winid wid, int nlines)
{
    int wh, ww, s_top, s_bottom;
    boolean border = curses_window_has_border(wid);
    WINDOW *win = curses_get_nhwin(wid);

    curses_get_window_size(wid, &wh, &ww);
    if (wh == 1) {
        curses_clear_nhwin(wid);
        return;
    }
    if (border) {
        s_top = 1;
        s_bottom = wh;
    } else {
        s_top = 0;
        s_bottom = wh - 1;
    }
    scrollok(win, TRUE);
    wsetscrreg(win, s_top, s_bottom);
    wscrl(win, nlines);
    scrollok(win, FALSE);
    if (wid == MESSAGE_WIN) {
        if (border)
            mx = 1;
        else
            mx = 0;
    }
    if (border) {
        box(win, 0, 0);
    }
    wrefresh(win);
}


/* Add given line to message history */
static void
mesg_add_line(const char* mline)
{
    mesg_add_line_ex(mline, (const char*)0, (const char*)0, ATR_NONE, NO_COLOR);
}

static void
mesg_add_line_ex(const char *mline, const char* attrs, const char* colors, int attr, int color)
{
    nhprev_mesg *current_mesg;

    /*
     * Messages are kept in a doubly linked list, with head 'first_mesg',
     * tail 'last_mesg', and a maximum capacity of 'max_messages'.
     */
    if (num_messages < max_messages) 
    {
        /* create a new list element */
        current_mesg = (nhprev_mesg *) alloc((unsigned) sizeof (nhprev_mesg));
        current_mesg->str = dupstr(mline);
        current_mesg->attrs = attrs ? cpystr(mline, attrs) : setstr(mline, attr);
        current_mesg->colors = colors ? cpystr(mline, colors) : setstr(mline, color);
    }
    else 
    {
        /* instead of discarding list element being forced out, reuse it */
        current_mesg = first_mesg;
        /* whenever new 'mline' is shorter, extra allocation size of the
           original element will be frittered away, until eventually we'll
           discard this 'str' and dupstr() a replacement; we could easily
           track the allocation size but don't really need to do so */
        if (strlen(mline) <= strlen(current_mesg->str))
        {
            Strcpy(current_mesg->str, mline);
            size_t len = strlen(mline);
            if(attrs)
                memcpy(current_mesg->attrs, attrs, len);
            else
                memset(current_mesg->attrs, attr, len);

            if (colors)
                memcpy(current_mesg->colors, colors, len);
            else
                memset(current_mesg->colors, color, len);

            current_mesg->attrs[len] = current_mesg->colors[len] = 0;
        }
        else 
        {
            free((genericptr_t) current_mesg->str);
            free((genericptr_t) current_mesg->attrs);
            free((genericptr_t) current_mesg->colors);
            current_mesg->str = dupstr(mline);
            current_mesg->attrs = attrs ? cpystr(mline, attrs) : setstr(mline, attr);
            current_mesg->colors = colors ? cpystr(mline, colors) : setstr(mline, color);
        }
    }
    current_mesg->turn = moves;

    if (num_messages == 0) 
    {
        /* very first message; set up head */
        first_mesg = current_mesg;
    } 
    else 
    {
        /* not first message; tail exists */
        last_mesg->next_mesg = current_mesg;
    }
    current_mesg->prev_mesg = last_mesg;
    last_mesg = current_mesg; /* new tail */

    if (num_messages < max_messages) 
    {
        /* wasn't at capacity yet */
        ++num_messages;
    }
    else 
    {
        /* at capacity; old head is being removed */
        first_mesg = first_mesg->next_mesg; /* new head */
        first_mesg->prev_mesg = NULL; /* head has no prev_mesg */
    }
    /* since 'current_mesg' might be reusing 'first_mesg' and has now
       become 'last_mesg', this update must be after head replacement */
    last_mesg->next_mesg = NULL; /* tail has no next_mesg */
}


/* Returns specified line from message history, or NULL if out of bounds */

static nhprev_mesg *
get_msg_line(boolean reverse, int mindex)
{
    int count;
    nhprev_mesg *current_mesg;

    if (reverse) {
        current_mesg = last_mesg;
        for (count = 0; count < mindex; count++) {
            if (!current_mesg)
                break;
            current_mesg = current_mesg->prev_mesg;
        }
    } else {
        current_mesg = first_mesg;
        for (count = 0; count < mindex; count++) {
            if (!current_mesg)
                break;
            current_mesg = current_mesg->next_mesg;
        }
    }
    return current_mesg;
}

/* save/restore code retrieves one ^P message at a time during save and
   puts it into save file; if any new messages are added to the list while
   that is taking place, the results are likely to be scrambled */
char*
curses_getmsghistory_ex(attrs_ptr, colors_ptr, init)
char** attrs_ptr, **colors_ptr;
boolean init;
{
    static int nxtidx;
    nhprev_mesg *mesg;

    if (init)
        nxtidx = 0;
    else
        ++nxtidx;

    if (nxtidx < num_messages) {
        /* we could encode mesg->turn with the text of the message,
           but then that text might need to be truncated, and more
           significantly, restoring the save file with another
           interface wouldn't know how find and decode or remove it;
           likewise, restoring another interface's save file with
           curses wouldn't find the expected turn info;
           so, we live without that */
        mesg = get_msg_line(FALSE, nxtidx);
    }
    else
    {
        mesg = (nhprev_mesg*)0;
    }

    if (mesg)
    {
        if (attrs_ptr)
            *attrs_ptr = mesg->attrs;
        if (colors_ptr)
            *colors_ptr = mesg->colors;
    }
    else
    {
        if (attrs_ptr)
            *attrs_ptr = (char*)0;
        if (colors_ptr)
            *colors_ptr = (char*)0;
    }

    return mesg ? mesg->str : (char *) 0;
}

/*
 * This is called by the core savefile restore routines.
 * Each time we are called, we stuff the string into our message
 * history recall buffer. The core will send the oldest message
 * first (actually it sends them in the order they exist in the
 * save file, but that is supposed to be the oldest first).
 * These messages get pushed behind any which have been issued
 * during this session since they come from a previous session
 * and logically precede anything (like "Restoring save file...")
 * that's happened now.
 *
 * Called with a null pointer to finish up restoration.
 *
 * It's also called by the quest pager code when a block message
 * has a one-line summary specified.  We put that line directly
 * into message history for ^P recall without having displayed it.
 */
void
curses_putmsghistory_ex(msg, attrs, colors, restoring_msghist)
const char *msg;
const char* attrs, *colors;
boolean restoring_msghist;
{
    static boolean initd = FALSE;
    static int stash_count;
    static nhprev_mesg *stash_head = 0;

    if (restoring_msghist && !initd) {
        /* hide any messages we've gathered since starting current session
           so that the ^P data will start out empty as we add ones being
           restored from save file; we'll put these back after that's done */
        stash_count = num_messages, num_messages = 0;
        stash_head = first_mesg, first_mesg = (nhprev_mesg *) 0;
        last_mesg = (nhprev_mesg *) 0; /* no need to remember the tail */
        initd = TRUE;
    }

    if (msg) {
        mesg_add_line_ex(msg, attrs, colors, ATR_NONE, NO_COLOR);
        /* treat all saved and restored messages as turn #1 */
        last_mesg->turn = 1L;
    } else if (stash_count) {
        nhprev_mesg *mesg;
        long mesg_turn;

        /* put any messages generated during the beginning of the current
           session back; they logically follow any from the previous
           session's save file */
        while (stash_count > 0) {
            /* we could manipulate the linked list directly but treating
               stashed messages as newly occurring ones is much simpler;
               we ignore the backlinks because the list is destroyed as it
               gets processed hence there can't be any other traversals */
            mesg = stash_head;
            stash_head = mesg->next_mesg;
            --stash_count;
            mesg_turn = mesg->turn;
            mesg_add_line_ex(mesg->str, mesg->attrs, mesg->colors, ATR_NONE, NO_COLOR);
            /* added line became new tail */
            last_mesg->turn = mesg_turn;
            free((genericptr_t) mesg->str);
            free((genericptr_t) mesg->attrs);
            free((genericptr_t) mesg->colors);
            free((genericptr_t) mesg);
        }
        initd = FALSE; /* reset */
    }
}

/*cursmesg.c*/
