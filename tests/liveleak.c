#include <stdio.h>

#include "test.h"

static const char
*urls[] = {
    "http://www.liveleak.com/view?i=704_1228511265",
    "http://www.liveleak.com/e/6ff_1228698283",
    NULL
};

int
main (int argc, char *argv[]) {
    register int i,rc;

    for (i=0,rc=0; urls[i] && !rc; ++i)
        rc = runtest_host("../src/cclive -n", urls[i]);

    return(rc);
}