
/*
 * Electric Fence - Red-Zone memory allocator.
 * Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>
 * Copyright (C) 2002-2004 Hayati Ayguen <hayati.ayguen@epost.de>, Procitec GmbH
 * License: GNU GPL (GNU General Public License, see COPYING-GPL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * FILE CONTENTS:
 * --------------
 * This is a small tool to generate the "efence_config.h" configuration file.
 * Its purpose is to allow fixed size memory allocation on stack, which can
 * get protected calling page protection functions.
 * Also its nice for the user to be able to see the page size.
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <stdarg.h>

#ifndef WIN32
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/mman.h>
#else
  #define WIN32_LEAN_AND_MEAN 1
  #include <windows.h>
  #include <winbase.h>

  typedef LPVOID caddr_t;
  typedef unsigned u_int;
#endif


/*
 * retrieve page size
 * size_t  Page_Size(void)
 */
static size_t
Page_Size(void)
{
#if defined(WIN32)
  SYSTEM_INFO SystemInfo;
  GetSystemInfo( &SystemInfo );
  return (size_t)SystemInfo.dwPageSize;
#elif defined(_SC_PAGESIZE)
	return (size_t)sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	return (size_t)sysconf(_SC_PAGE_SIZE);
#else
/* extern int	getpagesize(); */
	return getpagesize();
#endif
}


int main(int argc, char *argv[])
{
  unsigned long pagesize = Page_Size();

  printf("/*\n");
  printf(" * WARNING: DO NOT CHANGE THIS FILE!\n");
  printf(" * This file is automatically generate from createconf\n");

#if defined(WIN32)
  #if defined(_MSC_VER)
    printf(" * using Microsoft Visual C++ under MS Windows(TM) the %s\n", __DATE__ );
  #else
    printf(" * under MS Windows(TM) the %s\n", __DATE__ );
  #endif
#elif ( defined(sgi) )
    printf(" * under sgi the %s\n", __DATE__ );
#elif ( defined(_AIX) )
    printf(" * under AIX the %s\n", __DATE__ );
#elif ( defined(__linux__) )
    printf(" * under Linux the %s\n", __DATE__ );
#else
    printf(" * the %s \n", __DATE__ );
#endif

  printf(" */\n");
  printf("\n");
  printf("#ifndef _EFENCE_CONFIG_H_\n");
  printf("#define _EFENCE_CONFIG_H_\n");
  printf("\n");
  printf("/*\n");
  printf(" * Number of bytes per virtual-memory page, as returned by Page_Size().\n");
  printf(" */\n");
  printf("#define EF_PAGE_SIZE %lu\n", pagesize);
  printf("\n");
  printf("#endif /* _EFENCE_CONFIG_H_ */\n");
  return 0;
}