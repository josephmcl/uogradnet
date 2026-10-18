/*
 * nickserv/allowmail: only let people register with an email address
 * we know.
 *
 * Two lists decide:
 *
 *   allowmail_domains   in atheme.conf (nickserv block), so it lives in git:
 *                       allowmail_domains = "uoregon.edu uogradnet.xyz";
 *                       Matches the domain and any subdomain
 *                       (cs.uoregon.edu).
 *
 *   ALLOWMAIL           exceptions for alumni and guests, kept in the
 *                       services database and managed by services admins:
 *                       /msg NickServ ALLOWMAIL ADD friend@gmail.com alum
 *                       /msg NickServ ALLOWMAIL DEL friend@gmail.com
 *                       /msg NickServ ALLOWMAIL LIST
 *                       A pattern may use * and ? (*@example.org).
 *
 * Proving the address belongs to the person is the job of `auth = email`:
 * the account stays unverified until they send back the emailed code.
 *
 * Built inside the Atheme 7.2 source tree; see deploy/bootstrap.sh.
 */

#include "atheme.h"
#include "conf.h"

DECLARE_MODULE_V1
(
    "nickserv/allowmail", true, _modinit, _moddeinit,
    PACKAGE_STRING,
    "UOGradNet"
);

#define DB_ROW "ALM"

typedef struct {
    char  *pattern;
    time_t added;
    char  *creator;
    char  *reason;
} allowmail_entry;

static char *TheDomains = NULL;         /* "uoregon.edu uogradnet.xyz" */
static mowgli_list_t TheExceptions;     /* of allowmail_entry */

static void _check_registration(hook_user_register_check_t *data);
static void _command(sourceinfo_t *si, int parc, char *parv[]);
static void _write_database(database_handle_t *db);
static void _read_row(database_handle_t *db, const char *type);

command_t ns_allowmail = {
    "ALLOWMAIL",
    N_("Allows registrations from email addresses outside the usual domains."),
    PRIV_USER_ADMIN, 3, _command, { .path = "nickserv/allowmail" }
};

void _modinit(module_t *m) {
    if (!module_find_published("backend/opensex")) {
        slog(LG_INFO, "%s needs the opensex database backend; not loading.", m->name);
        m->mflags = MODTYPE_FAIL;
        return;
    }

    hook_add_event("user_can_register");
    hook_add_user_can_register(_check_registration);
    hook_add_db_write(_write_database);
    db_register_type_handler(DB_ROW, _read_row);
    add_dupstr_conf_item("ALLOWMAIL_DOMAINS", &nicksvs.me->conf_table, 0,
                         &TheDomains, NULL);
    service_named_bind_command("nickserv", &ns_allowmail);
}

void _moddeinit(module_unload_intent_t intent) {
    (void) intent;
    hook_del_user_can_register(_check_registration);
    hook_del_db_write(_write_database);
    db_unregister_type_handler(DB_ROW);
    del_conf_item("ALLOWMAIL_DOMAINS", &nicksvs.me->conf_table);
    service_named_unbind_command("nickserv", &ns_allowmail);
}

/* ---------------------------------------------------------- matching */

/* "me@cs.uoregon.edu" is in "uoregon.edu"; "me@notuoregon.edu" is not. */
static bool _domain_matches(const char *email_domain, const char *domain, size_t length) {
    size_t have;

    have = strlen(email_domain);
    if (have == length)
        return strncasecmp(email_domain, domain, length) == 0;
    if (have > length && email_domain[have - length - 1] == '.')
        return strncasecmp(email_domain + have - length, domain, length) == 0;
    return false;
}

static bool _in_domains(const char *email) {
    const char *at, *domain, *end;

    at = strrchr(email, '@');
    if (at == NULL || TheDomains == NULL)
        return false;

    for (domain = TheDomains; *domain != '\0'; domain = end) {
        while (*domain == ' ')
            ++domain;
        end = domain;
        while (*end != '\0' && *end != ' ')
            ++end;
        if (end > domain && _domain_matches(at + 1, domain, (size_t) (end - domain)))
            return true;
    }
    return false;
}

static bool _in_exceptions(const char *email) {
    mowgli_node_t *n;
    allowmail_entry *entry;

    MOWGLI_ITER_FOREACH(n, TheExceptions.head) {
        entry = n->data;
        if (match(entry->pattern, email) == 0)  /* match() returns 0 on a match */
            return true;
    }
    return false;
}

/* hook: user_can_register. Setting `approved` to nonzero refuses. */
static void _check_registration(hook_user_register_check_t *data) {
    if (data->approved)
        return;
    if (_in_domains(data->email) || _in_exceptions(data->email))
        return;

    command_fail(data->si, fault_noprivs,
        "Registration is limited to addresses at: %s. "
        "Alumni and guests can ask an admin in #help.",
        TheDomains != NULL ? TheDomains : "(none configured)");
    slog(LG_INFO, "REGISTER:ALLOWMAIL: refused \2%s\2 to \2%s\2 by \2%s\2",
         data->account, data->email,
         data->si->su != NULL ? data->si->su->nick : get_source_name(data->si));
    data->approved = 1;
}

/* ---------------------------------------------------------- database */

static void _free_entry(allowmail_entry *entry) {
    free(entry->pattern);
    free(entry->creator);
    free(entry->reason);
    free(entry);
}

static void _add_entry(const char *pattern, time_t added, const char *creator,
                       const char *reason) {
    allowmail_entry *entry;

    entry = smalloc(sizeof *entry);
    entry->pattern = sstrdup(pattern);
    entry->added = added;
    entry->creator = sstrdup(creator);
    entry->reason = sstrdup(reason);
    mowgli_node_add(entry, mowgli_node_create(), &TheExceptions);
}

static void _write_database(database_handle_t *db) {
    mowgli_node_t *n;
    allowmail_entry *entry;

    MOWGLI_ITER_FOREACH(n, TheExceptions.head) {
        entry = n->data;
        db_start_row(db, DB_ROW);
        db_write_word(db, entry->pattern);
        db_write_time(db, entry->added);
        db_write_word(db, entry->creator);
        db_write_str(db, entry->reason);
        db_commit_row(db);
    }
}

static void _read_row(database_handle_t *db, const char *type) {
    const char *pattern, *creator, *reason;
    time_t added;

    (void) type;
    pattern = db_sread_word(db);
    added   = db_sread_time(db);
    creator = db_sread_word(db);
    reason  = db_sread_str(db);
    _add_entry(pattern, added, creator, reason);
}

/* ---------------------------------------------------------- command */

static allowmail_entry *_find(const char *pattern, mowgli_node_t **node) {
    mowgli_node_t *n;
    allowmail_entry *entry;

    MOWGLI_ITER_FOREACH(n, TheExceptions.head) {
        entry = n->data;
        if (strcasecmp(entry->pattern, pattern) == 0) {
            if (node != NULL)
                *node = n;
            return entry;
        }
    }
    return NULL;
}

static void _add(sourceinfo_t *si, const char *pattern, const char *reason) {
    if (pattern == NULL || reason == NULL) {
        command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "ALLOWMAIL");
        command_fail(si, fault_needmoreparams, _("Syntax: ALLOWMAIL ADD <email> <reason>"));
        return;
    }
    if (strchr(pattern, '@') == NULL) {
        command_fail(si, fault_badparams, _("\2%s\2 is not an email address."), pattern);
        return;
    }
    if (_find(pattern, NULL) != NULL) {
        command_success_nodata(si, _("\2%s\2 is already allowed."), pattern);
        return;
    }
    _add_entry(pattern, CURRTIME, get_source_name(si), reason);
    logcommand(si, CMDLOG_ADMIN, "ALLOWMAIL:ADD: \2%s\2 (Reason: \2%s\2)", pattern, reason);
    command_success_nodata(si, _("\2%s\2 can now register."), pattern);
}

static void _del(sourceinfo_t *si, const char *pattern) {
    allowmail_entry *entry;
    mowgli_node_t *node;

    if (pattern == NULL) {
        command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "ALLOWMAIL");
        command_fail(si, fault_needmoreparams, _("Syntax: ALLOWMAIL DEL <email>"));
        return;
    }
    entry = _find(pattern, &node);
    if (entry == NULL) {
        command_fail(si, fault_nosuch_target, _("\2%s\2 is not on the list."), pattern);
        return;
    }
    logcommand(si, CMDLOG_ADMIN, "ALLOWMAIL:DEL: \2%s\2", entry->pattern);
    mowgli_node_delete(node, &TheExceptions);
    mowgli_node_free(node);
    _free_entry(entry);
    command_success_nodata(si, _("\2%s\2 removed. Accounts it already made are kept."), pattern);
}

static void _list(sourceinfo_t *si) {
    mowgli_node_t *n;
    allowmail_entry *entry;
    char when[BUFSIZE];
    struct tm tm;

    command_success_nodata(si, _("Domains (atheme.conf): %s"),
                           TheDomains != NULL ? TheDomains : "(none)");
    MOWGLI_ITER_FOREACH(n, TheExceptions.head) {
        entry = n->data;
        tm = *localtime(&entry->added);
        strftime(when, sizeof when, TIME_FORMAT, &tm);
        command_success_nodata(si, "\2%s\2: %s (%s, %s)",
                               entry->pattern, entry->reason, entry->creator, when);
    }
    command_success_nodata(si, _("End of list."));
    logcommand(si, CMDLOG_GET, "ALLOWMAIL:LIST");
}

static void _command(sourceinfo_t *si, int parc, char *parv[]) {
    const char *action = parv[0];

    (void) parc;
    if (action == NULL) {
        command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "ALLOWMAIL");
        command_fail(si, fault_needmoreparams, _("Syntax: ALLOWMAIL ADD|DEL|LIST [parameters]"));
        return;
    }
    if (strcasecmp(action, "ADD") == 0)
        _add(si, parv[1], parv[2]);
    else if (strcasecmp(action, "DEL") == 0)
        _del(si, parv[1]);
    else if (strcasecmp(action, "LIST") == 0)
        _list(si);
    else
        command_fail(si, fault_badparams, STR_INVALID_PARAMS, "ALLOWMAIL");
}
