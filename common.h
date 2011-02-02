/*
* Copyright 2010, 2011 Paul Chote
* This file is part of Rangahau, which is free software. It is made available
* to you under the terms of version 3 of the GNU General Public License, as
* published by the Free Software Foundation. For more information, see LICENSE.
*/

#include <stdarg.h>

#ifndef COMMON_H
#define COMMON_H

void rangahau_shutdown();
void rangahau_die(const char * format, ...);

#endif
