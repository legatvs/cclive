#include <stdio.h>
#include <stdlib.h>

#include "test.h"

#define TEST_URL \
    "http://www.dailymotion.com/hd/video/" \
        "x9fkzj_battlefield-1943-coral-sea-trailer_videogames"

static const char *formats[] = {
    "flv",
    "spak-mini",
    "vp6-hq",
    "vp6-hd",
    "vp6",
    "h264",
    NULL
};

int
main (int argc, char *argv[]) {
    register int i,rc;
    char *cmd;

    for (i=0,rc=0,cmd=0; formats[i] && !rc; ++i) {
        asprintf(&cmd, "../src/cclive -nf %s", formats[i]);
        rc = runtest_host(cmd, TEST_URL);
        free(cmd);
    }

    return(rc);
}