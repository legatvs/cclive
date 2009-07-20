#include <stdio.h>
#include <stdlib.h>

#include "test.h"

#define TEST_URL\
    "http://video.golem.de/internet/2174/firefox-3.5-test.html"

static const char *
formats[] = {
    "flv",
    "high",
    "ipod",
    NULL
};

int
main (int argc, char *argv[]) {
    register int i,rc;
    char *opts;

    for (i=0,rc=0,opts=0; formats[i] && !rc; ++i) {
        asprintf(&opts, "--format=%s", formats[i]);
        rc = runtest_host(opts, TEST_URL);
        free(opts);
    }

    return (rc);
}