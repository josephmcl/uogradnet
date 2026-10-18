#include "io.h"
#include "irc.h"
#include "database.h"
#include "session.h"

#include <signal.h>
#include <unistd.h>

#define RETRY_SECONDS 5
#define RETRY_SECONDS_MAX 120

static void _on_signal(int number) {
    (void) number;
    Irc.stop();
}

static void _install_signals(void) {
    struct sigaction action = {0};

    action.sa_handler = _on_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
}

int main(void) {
    unsigned int delay, i;

    Settings.load();
    setvbuf(stderr, NULL, _IOLBF, 0);
    _install_signals();

    /* Connect, log until the connection drops, wait, try again.
       The wait doubles up to two minutes while the server is down. */
    delay = RETRY_SECONDS;
    while (!Irc.stopping()) {
        if (Irc.connect(Settings.info->host, Settings.info->port) == 0) {
            notice_c("connected to %s:%s",
                     Settings.info->host, Settings.info->port);
            delay = RETRY_SECONDS;
            Session.begin();
            Irc.run(Session.line);
            Irc.close();
        } else {
            warning_c("could not connect to %s:%s",
                      Settings.info->host, Settings.info->port);
        }
        if (Irc.stopping())
            break;

        notice_c("reconnecting in %us", delay);
        for (i = 0; i < delay && !Irc.stopping(); ++i)
            sleep(1);
        if (delay < RETRY_SECONDS_MAX)
            delay *= 2;
    }

    Database.free();
    notice_c("%s", "stopped");
    return 0;
}
