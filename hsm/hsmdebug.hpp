/******************************************************************************

    @file    hsmdebug.hpp
    @author  Rajmund Szymanski
    @date    12.10.2026
    @brief   This file contains optional debug output macros.

 ******************************************************************************

   Copyright (c) 2018-2022 Rajmund Szymanski. All rights reserved.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to
   deal in the Software without restriction, including without limitation the
   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
   sell copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.

 ******************************************************************************/

#ifndef HSM_DEBUGOUT_HPP_
#define HSM_DEBUGOUT_HPP_

/* -------------------------------------------------------------------------- */

// usage: EDEBUG("format", args...), WDEBUG(...), IDEBUG(...), TDEBUG(...)
// output level is selected with DEBUG macro: 1 - error ... 4 - trace

#if defined(DEBUG) && DEBUG > 0

	#if defined(_WIN32) && defined(__GNUC__)
		#include <windows.h>
		__attribute__((constructor))
		static void WIN32_ANSI_INIT()
		{
			HANDLE handle_ = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD  mode_   = 0;
			if (GetConsoleMode(handle_, &mode_))
				SetConsoleMode(handle_, mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
		}
	#endif

	#ifndef PRINTF
		#include <cstdio>
		#define PRINTF printf
	#endif

	#ifndef NOCOLORED
	#define DEBUGOUT( c, h, ... ) ( PRINTF("\x1b[%dm" h, c), PRINTF(__VA_ARGS__), PRINTF("\x1b[0m" "\n") )
	#else
	#define DEBUGOUT( c, h, ... ) ( PRINTF(""         h   ), PRINTF(__VA_ARGS__), PRINTF(""        "\n") )
	#endif

#endif

/* -------------------------------------------------------------------------- */

// 30 Black,   90 Bright Black
// 31 Red,     91 Bright Red
// 32 Green,   92 Bright Green
// 33 Yellow,  93 Bright Yellow
// 34 Blue,    94 Bright Blue
// 35 Magenta, 95 Bright Magenta
// 36 Cyan,    96 Bright Cyan
// 37 White,   97 Bright White

#if defined(DEBUG) && DEBUG >= 1 // ERROR
	#define EDEBUG( ... ) DEBUGOUT( 91, "[error] ",   __VA_ARGS__ )
#else
	#define EDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 2 // WARNING
	#define WDEBUG( ... ) DEBUGOUT( 93, "[warning] ", __VA_ARGS__ )
#else
	#define WDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 3 // INFO
	#define IDEBUG( ... ) DEBUGOUT( 32, "[info] ",    __VA_ARGS__ )
#else
	#define IDEBUG( ... ) do {} while (0)
#endif

#if defined(DEBUG) && DEBUG >= 4 // TRACE
	#define TDEBUG( ... ) DEBUGOUT( 36, "[trace] ",   __VA_ARGS__ )
#else
	#define TDEBUG( ... ) do {} while (0)
#endif

/* -------------------------------------------------------------------------- */

#endif//HSM_DEBUGOUT_HPP_
