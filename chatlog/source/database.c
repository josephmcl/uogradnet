#include "database.h"

#include <libpq-fe.h>

static PGconn *TheConnection = NULL;

static int _connected(void) {
    if (TheConnection != NULL && PQstatus(TheConnection) == CONNECTION_OK)
        return 1;

    if (TheConnection == NULL)
        TheConnection = PQconnectdb(Settings.info->database);
    else
        PQreset(TheConnection);

    if (PQstatus(TheConnection) != CONNECTION_OK) {
        warning_c("database: %s", PQerrorMessage(TheConnection));
        return 0;
    }
    notice_c("%s", "connected to database");
    return 1;
}

/* Run one parameterised statement. The caller PQclear()s the result.
   Returns NULL on failure, after one reconnect-and-retry. */
static PGresult *_execute(const char *sql, int count, const char *const *values) {
    PGresult *rv;
    ExecStatusType status;
    int attempt;

    for (attempt = 0; attempt < 2; ++attempt) {
        if (!_connected())
            continue;
        rv = PQexecParams(TheConnection, sql, count, NULL, values, NULL, NULL, 0);
        status = PQresultStatus(rv);
        if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
            return rv;
        warning_c("database: %s", PQerrorMessage(TheConnection));
        PQclear(rv);
        if (PQstatus(TheConnection) == CONNECTION_OK)
            return NULL; /* a real SQL error; retrying won't help */
    }
    return NULL;
}

void database_record(const char *time, const char *channel, const char *nick,
                     const char *account, const char *kind, const char *body) {
    const char *values[6] = { time, channel, nick, account, kind, body };
    PGresult *result;

    result = _execute(
        "INSERT INTO messages (ts, channel, nick, account, kind, body) "
        "VALUES (COALESCE($1::timestamptz, now()), $2, $3, $4, $5, $6)",
        6, values);
    if (result == NULL) {
        error_c("could not record %s in %s", kind, channel);
        return;
    }
    PQclear(result);
}

void database_saved_channels(void ( *each)(const char *channel)) {
    PGresult *result;
    int i;

    result = _execute("SELECT name FROM bot_channels", 0, NULL);
    if (result == NULL)
        return;
    for (i = 0; i < PQntuples(result); ++i)
        each(PQgetvalue(result, i, 0));
    PQclear(result);
}

void database_remember_channel(const char *channel) {
    const char *values[1] = { channel };
    PGresult *result;

    result = _execute(
        "INSERT INTO bot_channels (name) VALUES ($1) ON CONFLICT DO NOTHING",
        1, values);
    if (result != NULL)
        PQclear(result);
}

void database_forget_channel(const char *channel) {
    const char *values[1] = { channel };
    PGresult *result;

    result = _execute(
        "DELETE FROM bot_channels WHERE lower(name) = lower($1)", 1, values);
    if (result != NULL)
        PQclear(result);
}

void database_free(void) {
    if (TheConnection != NULL)
        PQfinish(TheConnection);
    TheConnection = NULL;
}

const struct database Database = {
    .record           = database_record,
    .saved_channels   = database_saved_channels,
    .remember_channel = database_remember_channel,
    .forget_channel   = database_forget_channel,
    .free             = database_free
};
