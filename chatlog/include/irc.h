#pragma once

#include "io.h"

#define IRC_LINE 16384        /* 8191 bytes of tags + a 512 byte message */
#define IRC_PING_SECONDS 240  /* ping the server after this much silence */

/* One TCP connection to the server. The connection is plain text on
   purpose: chatlog only ever talks to 127.0.0.1.                     */
struct irc {
    int  ( *connect)(const char *host, const char *port);
    void ( *send)(const char *format, ...);
    /* Calls on_line for every line until the connection drops or
       stop() is called. Returns when that happens.                   */
    void ( *run)(void ( *on_line)(char *line));
    void ( *stop)(void);
    int  ( *stopping)(void);
    void ( *close)(void);
};

extern const struct irc Irc;
