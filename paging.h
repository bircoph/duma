/*
 * DUMA - Red-Zone memory allocator.
 * Copyright (C) 2002-2009 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
 * Copyright (C) 2006 Michael Eddington <meddington@gmail.com>
 * Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>
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
 * internal implementation file
 * contains system/platform dependent paging functions
 */


#ifndef DUMA_PAGING_H
#define DUMA_PAGING_H


/*
 * Lots of systems are missing the definition of PROT_NONE.
 */
#ifndef  PROT_NONE
#define  PROT_NONE  0
#endif

/*
 * 386 BSD has MAP_ANON instead of MAP_ANONYMOUS.
 */
#if ( !defined(MAP_ANONYMOUS) && defined(MAP_ANON) )
#define  MAP_ANONYMOUS  MAP_ANON
#endif


/* declarations */
#include "print.h"


/*
 * For some reason, I can't find mprotect() in any of the headers on
 * IRIX or SunOS 4.1.2
 */
/* extern C_LINKAGE int mprotect(void * addr, size_t len, int prot); */

#if !defined(WIN32)
static void * startAddr = (void*)0;
#endif

/* Function: stringErrorReport
 *
 * Get formatted error string and return.  For WIN32
 * FormatMessage is used, strerror all else.
 */
static const char * stringErrorReport(void)
{
#if defined(WIN32)
  DWORD LastError;
  LPVOID lpMsgBuf;

  LastError = GetLastError();
  FormatMessage(
				  FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
				, NULL
				, LastError
				, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)   /* Default language */
				, (LPTSTR) &lpMsgBuf
				, 0
				, NULL
			   );
  return (char*)lpMsgBuf; /* "Unknown error.\n"; */
#else
# ifndef DUMA_NO_STRERROR
  static int failing;

  if ( !failing )
  {
	const char *str;

	failing++;
	str = strerror(errno);
	failing--;

	if ( str != NULL )
	  return str;
  }
# endif

  return DUMA_strerror(errno);
#endif
}

/* Function: mprotectFailed
 *
 * Report that VirtualProtect or mprotect failed and abort
 * program execution.
 */
static void mprotectFailed(void)
{
#if defined(WIN32)
  DUMA_Abort("VirtualProtect() failed: %s", stringErrorReport());
#else
  DUMA_Abort("mprotect() failed: %s.\nCheck README section 'MEMORY USAGE AND EXECUTION SPEED'\n  your (Linux) system may limit the number of different page mappings per process", stringErrorReport());
#endif
}


/* Function: Page_Create
 *
 * Create memory.  Allocates actual memory. Uses
 * VirtualAlloc on windows and mmap on unix.
 *
 * See Also:
 *  <Page_Delete>
 */
static void *
Page_Create(size_t size, int exitonfail, int printerror)
{
  void * allocation;

#if defined(WIN32)

  allocation = VirtualAlloc(
							  NULL                    /* address of region to reserve or commit */
							, (DWORD) size            /* size of region */
							, (DWORD) MEM_COMMIT      /* type of allocation */
							, (DWORD) PAGE_READWRITE  /* type of access protection */
							);

  if ( (void*)0 == allocation )
  {
	if ( exitonfail )
	  DUMA_Abort("VirtualAlloc(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
	else if ( printerror )
	  DUMA_Print("\nDUMA warning: VirtualAlloc(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
  }


#elif defined(MAP_ANONYMOUS)

  /*
   * In this version, "startAddr" is a _hint_, not a demand.
   * When the memory I map here is contiguous with other
   * mappings, the allocator can coalesce the memory from two
   * or more mappings into one large contiguous chunk, and thus
   * might be able to find a fit that would not otherwise have
   * been possible. I could _force_ it to be contiguous by using
   * the MMAP_FIXED flag, but I don't want to stomp on memory mappings
   * generated by other software, etc.
   */
  allocation = mmap(
								startAddr
							  , (int)size
							  , PROT_READ|PROT_WRITE
							  , MAP_PRIVATE|MAP_ANONYMOUS
							  , -1
							  , 0
							  );

  #ifndef  __hpux
	/*
	 * Set the "address hint" for the next mmap() so that it will abut
	 * the mapping we just created.
	 *
	 * HP/UX 9.01 has a kernel bug that makes mmap() fail sometimes
	 * when given a non-zero address hint, so we'll leave the hint set
	 * to zero on that system. HP recently told me this is now fixed.
	 * Someone please tell me when it is probable to assume that most
	 * of those systems that were running 9.01 have been upgraded.
	 */
	startAddr = allocation + size;
  #endif

  if ( allocation == (void*)-1 )
  {
	allocation = (void*)0;
	if ( exitonfail )
	  DUMA_Abort("mmap(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
	else if ( printerror )
	  DUMA_Print("\nDUMA warning: mmap(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
  }

#else
  static int  devZeroFd = -1;

  if ( devZeroFd == -1 )
  {
	devZeroFd = open("/dev/zero", O_RDWR);
	if ( devZeroFd < 0 )
	  DUMA_Abort( "open() on /dev/zero failed: %s", stringErrorReport() );
  }

  /*
   * In this version, "startAddr" is a _hint_, not a demand.
   * When the memory I map here is contiguous with other
   * mappings, the allocator can coalesce the memory from two
   * or more mappings into one large contiguous chunk, and thus
   * might be able to find a fit that would not otherwise have
   * been possible. I could _force_ it to be contiguous by using
   * the MMAP_FIXED flag, but I don't want to stomp on memory mappings
   * generated by other software, etc.
   */
  allocation = mmap(
								startAddr
							  , (int)size
							  , PROT_READ|PROT_WRITE
							  , MAP_PRIVATE
							  , devZeroFd
							  , 0
							  );

  startAddr = allocation + size;

  if ( allocation == (void*)-1 )
  {
	allocation = (void*)0;
	if ( exitonfail )
	  DUMA_Abort("mmap(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
	else if ( printerror )
	  DUMA_Print("\nDUMA warning: mmap(%d) failed: %s", (DUMA_SIZE)size, stringErrorReport());
  }

#endif

// TESTING
//  memset((void*)allocation, 0, startAddr);

  return (void *)allocation;
}




/* Function: Page_AllowAccess
 *
 * Allow memory access to allocated memory.
 *
 * See Also:
 *  <Page_DenyAccess>
 */
void Page_AllowAccess(void * address, size_t size)
{
#if defined(WIN32)
  SIZE_T OldProtect, retQuery;
  MEMORY_BASIC_INFORMATION MemInfo;
  size_t tail_size;
  BOOL ret;

  while (size >0)
  {
	retQuery = VirtualQuery(address, &MemInfo, sizeof(MemInfo));
	if (retQuery < sizeof(MemInfo))
	  DUMA_Abort("VirtualQuery() failed\n");
	tail_size = (size > MemInfo.RegionSize) ? MemInfo.RegionSize : size;
	ret = VirtualProtect(
						  (LPVOID) address        /* address of region of committed pages */
						, (DWORD) tail_size       /* size of the region */
						, (DWORD) PAGE_READWRITE  /* desired access protection */
						, (PDWORD) &OldProtect    /* address of variable to get old protection */
						);
	if (0 == ret)
	  mprotectFailed();

	address = ((char *)address) + tail_size;
	size -= tail_size;
  }

#else
  if ( mprotect(address, size, PROT_READ|PROT_WRITE) < 0 )
	mprotectFailed();
#endif
}


/* Function: Page_DenyAccess
 *
 * Deny access to allocated memory region.
 *
 * See Also:
 *  <Page_AllowAccess>
 */
static void Page_DenyAccess(void * address, size_t size)
{
#if defined(WIN32)
  SIZE_T OldProtect, retQuery;
  MEMORY_BASIC_INFORMATION MemInfo;
  size_t tail_size;
  BOOL ret;

  while (size >0)
  {
	retQuery = VirtualQuery(address, &MemInfo, sizeof(MemInfo));
	if (retQuery < sizeof(MemInfo))
	  DUMA_Abort("VirtualQuery() failed\n");
	tail_size = (size > MemInfo.RegionSize) ? MemInfo.RegionSize : size;
	ret = VirtualProtect(
						  (LPVOID) address        /* address of region of committed pages */
						, (DWORD) tail_size       /* size of the region */
						, (DWORD) PAGE_NOACCESS   /* desired access protection */
						, (PDWORD) &OldProtect    /* address of variable to get old protection */
						);
	if (0 == ret)
	  mprotectFailed();

	address = ((char *)address) + tail_size;
	size -= tail_size;
  }

#else
  if ( mprotect(address, size, PROT_NONE) < 0 )
	mprotectFailed();
#endif
}


/* Function: Page_Delete
 *
 * Free's DUMA allocated memory.  This is the real deal, make sure
 * the page is no longer in our slot list first!
 *
 * See Also:
 *  <Page_Create>
 */
static void Page_Delete(void * address, size_t size)
{
#if defined(WIN32)

  void * alloc_address  = address;
  SIZE_T retQuery;
  MEMORY_BASIC_INFORMATION MemInfo;
  BOOL ret;

  /* release physical memory commited to virtual address space */
  while (size >0)
  {
	retQuery = VirtualQuery(address, &MemInfo, sizeof(MemInfo));

	if (retQuery < sizeof(MemInfo))
	  DUMA_Abort("VirtualQuery() failed\n");

	if ( MemInfo.State == MEM_COMMIT )
	{
	  ret = VirtualFree(
		(LPVOID) MemInfo.BaseAddress /* base of committed pages */
		, (DWORD) MemInfo.RegionSize   /* size of the region */
		, (DWORD) MEM_DECOMMIT         /* type of free operation */
		);

	  if (0 == ret)
		DUMA_Abort("VirtualFree(,,MEM_DECOMMIT) failed: %s", stringErrorReport());
	}

	address = ((char *)address) + MemInfo.RegionSize;
	size -= MemInfo.RegionSize;
  }

  /* release virtual address space */
  ret = VirtualFree(
	(LPVOID) alloc_address
	, (DWORD) 0
	, (DWORD) MEM_RELEASE
	);

  if (0 == ret)
	DUMA_Abort("VirtualFree(,,MEM_RELEASE) failed: %s", stringErrorReport());

#else

  if ( munmap(address, size) < 0 )
	Page_DenyAccess(address, size);

#endif
}


/* Function: Page_Size
 *
 * Retrieve page size.
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
  return getpagesize();
#endif
}

#endif /* DUMA_PAGING_H */
