/* cclive
 * Copyright (C) 2010-2013  Toni Gundogdu <legatvs@gmail.com>
 *
 * This file is part of cclive <http://cclive.sourceforge.net/>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef cclive_application_h
#define cclive_application_h

#include <cstdlib>

namespace cc
{

class application
{
public:
  typedef enum {ok=EXIT_SUCCESS, error=EXIT_FAILURE} exit_status;
public:
  inline application():_curl(NULL) { }
  inline virtual ~application()    { }
public:
  exit_status exec(int,char **);
private:
  void *_curl;
};

} // namespace cc

#endif // cclive_application_h

// vim: set ts=2 sw=2 tw=72 expandtab:
