#pragma once

/* Everything chatlog needs to know, read once from the environment.
   deploy/deploy.sh writes these to /etc/uogradnet/chatlog.env.

     IRC_HOST, IRC_PORT    server (default 127.0.0.1:6667)
     IRC_NICK              nickname (default "chatlog")
     IRC_ACCOUNT           services account for SASL (default: IRC_NICK)
     IRC_PASSWORD          services password; empty means no SASL
     IRC_REGISTER_EMAIL    if set, create the account on first run
     IRC_CHANNELS          comma separated channels to always join
     ALLOW_INVITES         "1" joins channels the bot is /INVITEd to
     DATABASE_URL          libpq conninfo (default "dbname=chatlog")
     LOG_LEVEL             "debug" prints raw IRC traffic            */

typedef struct {
    const char *host;
    const char *port;
    const char *nick;
    const char *account;
    const char *password;
    const char *email;
    const char *channels;
    const char *database;
    int invites;
    int debug;
} settings_info;

struct settings {
    settings_info *info;
    void ( *load)(void);
};

extern const struct settings Settings;
