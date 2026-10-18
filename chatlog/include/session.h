#pragma once

#include "io.h"
#include "message.h"

/* The IRC side of one connection: capability negotiation, SASL,
   joining channels, and turning channel events into rows. */
struct session {
    void ( *begin)(void);           /* call right after connecting */
    void ( *line)(char *line);      /* call for every server line  */
};

extern const struct session Session;
