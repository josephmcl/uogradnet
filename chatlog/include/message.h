#pragma once

#include "io.h"

#define MESSAGE_PARAMETERS 16
#define MESSAGE_NICK 64

/* One line from the server, split in place:

     @time=...;account=... :nick!user@host COMMAND p0 p1 :trailing

   Every pointer points into the line it was parsed from.            */
typedef struct {
    char *time;                         /* server-time tag, or NULL  */
    char *account;                      /* account tag, or NULL      */
    char *prefix;                       /* nick!user@host, or NULL   */
    char  nick[MESSAGE_NICK];           /* the nick part of prefix   */
    char *command;
    char *parameters[MESSAGE_PARAMETERS];
    int   count;                        /* number of parameters      */
} irc_message;

/* returns 0 on success, -1 for a line that isn't a message */
int message_parse(char *line, irc_message *message);
