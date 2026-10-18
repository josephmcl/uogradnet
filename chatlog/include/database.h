#pragma once

#include "io.h"

/* The chat log in PostgreSQL. Schema: ../schema.sql

   Every call reconnects first if the connection was lost (for
   example after PostgreSQL restarts), and retries once.            */
struct database {
    /* time is the server-time tag ("2026-10-18T04:49:54.040Z") or
       NULL for now(); account and body may be NULL.                */
    void ( *record)(const char *time, const char *channel,
                    const char *nick, const char *account,
                    const char *kind, const char *body);
    /* calls each() for every channel the bot was invited into */
    void ( *saved_channels)(void ( *each)(const char *channel));
    void ( *remember_channel)(const char *channel);
    void ( *forget_channel)(const char *channel);
    void ( *free)(void);
};

extern const struct database Database;
