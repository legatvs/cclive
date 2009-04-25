/*
 * cclive Copyright (C) 2009 Toni Gundogdu. This file is part of cclive.
 * 
 * cclive is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 * 
 * cclive is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include <tr1/memory>
#include <cstdlib>

#include <sys/wait.h>

#ifdef USE_SIGWINCH
 #include <signal.h>
 #ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
 #endif
#endif

#include "except.h"
#include "video.h"
#include "singleton.h"
#include "cmdline.h"
#include "opts.h"
#include "macros.h"
#include "util.h"
#include "exec.h"
#include "progressbar.h"

#ifdef USE_SIGWINCH
static volatile sig_atomic_t recv_sigwinch;

RETSIGTYPE
handle_sigwinch(int sig) {
    recv_sigwinch = 1;
    signal(SIGWINCH, handle_sigwinch);
}

static int
getTermWidth() {
    int fd = fileno(stderr);
    winsize wsz;

    if (ioctl(fd, TIOCGWINSZ, &wsz) < 0)
        return 0;

    return wsz.ws_col;
}
#endif

ProgressBar::ProgressBar()
    : props(VideoProperties()), lastUpdate(0),
      started(0),               initial(0),
      total(0),                 count(0),
      done(false),              width(0),
      termWidth(0),             streamFlag(false),
      streamPid(-1)
{
}

void
ProgressBar::init(VideoProperties& props) {
    this->props = props;

    initial = props.getInitial();   // bytes dl previously
    total = props.getLength();      // expected bytes

    if (initial > total)
        total = initial;

#ifdef USE_SIGWINCH
    if (!termWidth || recv_sigwinch) {
        termWidth = getTermWidth();
        if (!termWidth)
            termWidth = DEFAULT_TERM_WIDTH;
    }
#else
    termWidth = DEFAULT_TERM_WIDTH;
#endif
    width = termWidth-1; // do not use the last column
    time(&started);
}

void
ProgressBar::update(double now) {
    static const double REFRESH_INTERVAL = 0.2;
    bool force_update = false;
#ifdef USE_SIGWINCH
    if (recv_sigwinch) {
        const int old_width = termWidth;
        termWidth = getTermWidth();
        if (!termWidth)
            termWidth = DEFAULT_TERM_WIDTH;
        if (termWidth != old_width) {
            width = termWidth - 1;
            force_update = true;
        }
        recv_sigwinch = 0;
    }
#endif
    time_t tnow;
    time(&tnow);

    const time_t elapsed = tnow - started;

    if (!done) {
        if ((elapsed - lastUpdate) < REFRESH_INTERVAL && !force_update)
            return;
    }
    else
        now = total;

    lastUpdate = elapsed;
    const double size = initial+now;

    std::stringstream b;
    b.setf(std::ios::fixed);

    register int l = 32;
    if (width > DEFAULT_TERM_WIDTH)
        l += width - DEFAULT_TERM_WIDTH;
    b << props.getFilename().substr(0,l);

    if (total > 0) {
        const double _size = !done ? size:now;
        const int percent = 100.0 * size / total;
        if (percent < 100)
            b << "  " << std::setw(2) << percent << "% ";
        else
            b << "  100%";
        b << "  "
          << std::setw(4)
          << std::setprecision(1)
          << _TOMB(_size)
          << "M / "
          << std::setw(4)
          << std::setprecision(1)
          << _TOMB(total)
          << "M";

        Options opts = optsmgr.getOptions();
        if (opts.stream_given
            && opts.stream_exec_given
            && !streamFlag)
        {
            if (percent >= opts.stream_arg)
                forkStreamer();
        }
    }

    std::stringstream tmp;
    tmp.setf(std::ios::fixed);

    double rate = elapsed ? (now/elapsed):0;

    if (rate > 0) {
        std::string eta;
        if (!done) {
            const int left = static_cast<int>((total-now)/rate);
            eta = timeToStr(left);
        }
        else
            eta = timeToStr(elapsed);
        std::string unit = getUnit(rate);
        tmp << "  "
            << std::setw(4)
            << std::setprecision(1)
            << rate
            << unit
            << "  "
            << std::setw(6)
            << eta;
    }
    else
        tmp << "  --.-K/s  --:--";

    l = width - tmp.str().length(); // pad to term width
    for (register int i=b.str().length(); i<l; ++i)
        b << " ";

    b << tmp.str();

    std::cout << "\r" << b.str() << std::flush;
    count = now;
}

void
ProgressBar::finish() {
    if (total > 0
        && count + initial > total)
    {
        total = initial + count;
    }
    done = true;
    update(-1);

    if (streamFlag) {
        if (waitpid(streamPid, 0, 0) != streamPid)
            perror("waitpid");
        streamFlag = false;
    }
}

const std::string
ProgressBar::timeToStr(const int secs) const {
    std::stringstream s;

    if (secs < 100)
        s << secs << "s";
    else if (secs < 100 * 60)
        s << secs/60 << "m" << secs%60 << "s";
    else if (secs < 48 * 3600)
        s << secs/3600 << "h" << (secs/60)%60 << "m";
    else if (secs < 100 * 86400)
        s << secs/86400 << "d" << (secs/3600)%60 << "h";
    else
        s << secs/86400 << "d";

    return s.str();
}

const std::string
ProgressBar::getUnit(double& rate) const {
    static const char *units[] = {"K/s", "M/s", "G/s"};
    register int i = 0;
    if (rate < 1024*1024) {
        rate /= 1024;
    }
    else if (rate < 1024*1024) {
        rate /= 1024*1024;
        i = 1;
    }
    else if (rate < 1024*1024*1024) {
        rate /= 1024*1024*1024;
        i = 2;
    }
    return units[i];
}

void
ProgressBar::forkStreamer() {
    streamFlag = true;
#if defined(HAVE_FORK) && defined(HAVE_WORKING_FORK)
    if ((streamPid = fork()) < 0)
        perror("fork");
    else if (streamPid == 0) {
        execmgr.playStream(props);
        exit(0);
    }
#endif
}