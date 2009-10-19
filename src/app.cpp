/*
 * Copyright (C) 2009 Toni Gundogdu.
 *
 * This file is part of cclive.
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

#ifndef HAVE_PTRDIFF_T
#error Cannot compile without ptrdiff_t support
#endif

#include <iostream>
#include <vector>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <tr1/memory>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "macros.h"
#include "opts.h"
#include "quvi.h"
#include "curl.h"
#include "util.h"
#include "exec.h"
#include "retry.h"
#include "log.h"
#include "app.h"

#if defined (SIGWINCH) && defined (TIOCGWINSZ)
#define WITH_RESIZE
#endif

#define SHP std::tr1::shared_ptr

// singleton instances
static SHP<OptionsMgr> __optsmgr (new OptionsMgr);
static SHP<QuviMgr>    __quvimgr (new QuviMgr);
static SHP<CurlMgr>    __curlmgr (new CurlMgr);
static SHP<ExecMgr>    __execmgr (new ExecMgr);
static SHP<RetryMgr>   __retrymgr(new RetryMgr);
static SHP<LogMgr>     __logmgr  (new LogMgr);

extern void handle_sigwinch(int); // src/progress.cpp

App::~App() {
}

void
App::main(const int& argc, char * const *argv) {
    optsmgr.init(argc, argv);
    logmgr.init();  // apply --quiet
    quvimgr.init(); // creates also curl handle which we'll reuse
    curlmgr.init();
}

static void
print_video(const QuviVideo& props) {
    logmgr.cout()
        << "file: "
        << props.getFilename()
        << "  "
        << std::setprecision(1)
        << _TOMB(props.getLength())
        << "M  ["
        << props.getContentType()
        << "]"
        << std::endl;
}

static void
print_csv(const QuviVideo& props) {
    std::cout.setf(std::ios::fixed);
    std::cout.unsetf(std::ios::showpoint);
    std::cout
        << "csv:\""
        << props.getFilename()
        << "\",\""
        << std::setprecision(0)
        << props.getLength()
        << "\",\""
        << props.getLink()
        << "\""
        << std::endl;
}

static void
fetch_page(QuviVideo& props,
          const bool& reset=false)
{
    if (reset)
        retrymgr.reset();
    try { props.parse(); }
    catch (const QuviException& x) {
        retrymgr.handle(x);
        fetch_page(props);
    }
    logmgr.resetReturnCode();
}

static void
fetch_file(QuviVideo& props, const bool& reset=false) {
    if (reset)
        retrymgr.reset();
    try   { curlmgr.fetchToFile(props); }
    catch (const QuviException& x) {
        retrymgr.setRetryUntilRetrievedFlag();
        retrymgr.handle(x);
        fetch_file(props);
    }
    logmgr.resetReturnCode();
}

static void
report_notice() {
    static const char report_notice[] =
    ":: A bug? If you think so, and you can reproduce the above,\n"
    ":: consider submitting it to the issue tracker:\n"
    "::     <http://code.google.com/p/cclive/issues/>\n";
    logmgr.cerr() << report_notice << std::endl;
}

static void
handle_url(const std::string& url) {
    try
    {
        QuviVideo props(url);

        try
        {
            fetch_page(props);

            const Options opts =
                optsmgr.getOptions();

            if (opts.no_extract_given)
                print_video(props);
            else if (opts.emit_csv_given)
                print_csv(props);
            else if (opts.stream_pass_given)
                execmgr.passStream(props);
            else
            {
                if (opts.print_fname_given)
                    print_video(props);

                fetch_file(props, true);
            }

            if (optsmgr.getOptions().exec_run_given) 
                execmgr.append(props);
        }
        catch (const NothingToDoException& x) 
            { logmgr.cerr(x, false); }
    }
    catch (const NoSupportException& x)
        { logmgr.cerr(x, false); }
    catch (const ParseException& x)
        { logmgr.cerr(x, false); report_notice(); }
    catch (const QuviException& x)
        { /* printed by retrymgr.handle already */ }
}

typedef std::vector<std::string> STRV;

void
App::run() {

    const Options opts =
        optsmgr.getOptions();

    if (opts.version_given) {
        printVersion();
        return;
    }

    if (opts.hosts_given) {
        char *domain=0, *formats=0;
        while (quvi_iter_host(&domain, &formats) != QUVI_LASTHOST)
            std::cout << domain << "\t" << formats << "\n";
        std::cout
            << "\nNote: Some videos may have limited number "
            << "of formats available." << std::endl;
        return;
    }

    execmgr.verifyExecArgument();

#if !defined(HAVE_FORK) || !defined(HAVE_WORKING_FORK)
    if (opts.stream_exec_given) {
        logmgr.cerr()
            << "warn: this system does not have a working fork.\n"
            << "warn: --stream-exec ignored."
            << std::endl;
    }
#endif

    STRV tokens;

    typedef unsigned int _uint;

    if (!opts.inputs_num)
        tokens = parseInput();
    else {
        for (register _uint i=0; i<opts.inputs_num; ++i)
            tokens.push_back(opts.inputs[i]);
    }

    for (STRV::iterator iter=tokens.begin();
        iter != tokens.end();
        ++iter)
    {
        // Convert alternate domain link to youtube.com page link.
        Util::nocookieToYoutube(*iter);

        // Convert any embed type URLs to video page links.
        Util::embedToPage(*iter);

        // Convert last.fm video link to Youtube page link.
        if ((*iter).find("last.fm") != std::string::npos)
            Util::lastfmToYoutube(*iter);
    }

    logmgr.cout().setf(std::ios::fixed);
#ifdef WITH_RESIZE
    signal(SIGWINCH, handle_sigwinch);
#endif
    std::for_each(tokens.begin(), tokens.end(), handle_url);

    if (opts.exec_run_given)
        execmgr.playQueue();
}

STRV
App::parseInput() {
    std::string input;

    char ch;
    while (std::cin.get(ch))
        input += ch;

    std::istringstream iss(input);
    STRV tokens;

    std::copy(
        std::istream_iterator<std::string >(iss),
        std::istream_iterator<std::string >(),
        std::back_inserter<STRV>(tokens)
    );

    return tokens;
}

void
App::printVersion() {
static const char copyr_notice[] =
"Copyright (C) 2009 Toni Gundogdu. "
"License GPLv3+: GNU GPL version 3 or later\n"
"This is free software; see the  source for  copying conditions.  There is NO\n"
"warranty;  not even for MERCHANTABILITY or FITNESS FOR A  PARTICULAR PURPOSE.";

    std::cout
        << CMDLINE_PARSER_PACKAGE       << " version "
        << CMDLINE_PARSER_VERSION       << " with libquvi version "
        << quvi_version(QUVI_VERSION)   << "  ["
#ifdef BUILD_DATE
        << BUILD_DATE << "-"
#endif
        << CANONICAL_TARGET             << "]\n"
        << copyr_notice                 << "\n"
        << "\n  Locale/codeset  : "     << optsmgr.getLocale()
        << "\n  Config          : "     << optsmgr.getPath()
        << "\n  Features        : "
#ifdef WITH_RESIZE
        << "sigwinch "
#endif
        << "\n  Home            : "     << "<http://cclive.googlecode.com/>"
        << std::endl;
}


