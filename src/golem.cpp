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

#include <string>
#include <vector>

#include "error.h"
#include "except.h"
#include "video.h"
#include "util.h"
#include "singleton.h"
#include "cmdline.h"
#include "opts.h"
#include "curl.h"
#include "hosthandler.h"

GolemHandler::GolemHandler()
    : HostHandler()
{
    props.setHost   ("golem");
    props.setDomain ("golem.de");
    props.setFormats("flv|ipod|high");
}

const bool
GolemHandler::isHost(std::string url) {
    return Util::toLower(url).find(props.getDomain())
        != std::string::npos;
}

void
GolemHandler::parseId() {
    const char *begin = "\"id\", \"";
    const char *end   = "\"";
    props.setId( Util::subStr(pageContent, begin, end) );
}

void
GolemHandler::parseLink() {
    std::string config_url = "http://video.golem.de/xml/" + props.getId();

    std::string config =
        curlmgr.fetchToMem(config_url, "config");

    std::string link =
        "http://video.golem.de/download/" + props.getId();

    std::string title = 
        Util::subStr(config, "<title>", "</title>");

    std::string fmt = optsmgr.getOptions().format_arg;

    if (fmt == "best") {
        // One should not simply assume "high" (or "hd")
        // is necessarily the best format available.
        // See clive (Golem.pm) for an example.
        fmt = "high";
    }
    else {
        // host uses "medium" for default ("flv" in clive terms)
        if (fmt == "flv")
            fmt = ""; 
    }

    if (!fmt.empty())
        link += "?q=" + fmt;

    props.setLink(link);
}


