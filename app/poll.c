
/*
 * The Real SoundTracker - poll() emulation using select()
 *
 * Copyright (C) 1994, 1996, 1997 Free Software Foundation, Inc.
 * Copyright (C) 1998 Michael Krause
 * Part of this file was part of the GNU C Library.
 *
 * Copyright (C) 2000 Fabian Giesen (pipe emulation code)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#ifndef HAVE_POLL

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "poll.h"

/* Poll the file descriptors described by the NFDS structures starting at
   FDS.  If TIMEOUT is nonzero and not -1, allow TIMEOUT milliseconds for
   an event to occur; if TIMEOUT is -1, block until an event occurs.
   Returns the number of file descriptors with events, zero if timed out,
   or -1 for errors.  */

int
poll (fds, nfds, timeout)
     struct pollfd *fds;
     unsigned int nfds;
     int timeout;
{
  struct timeval tv;
  fd_set rset, wset, xset;
  struct pollfd *f;
  int ready;
  int maxfd = 0;

  FD_ZERO (&rset);
  FD_ZERO (&wset);
  FD_ZERO (&xset);

  for (f = fds; f < &fds[nfds]; ++f)
    if (f->fd >= 0)
      {
	if (f->events & POLLIN)
	  FD_SET (f->fd, &rset);
	if (f->events & POLLOUT)
	  FD_SET (f->fd, &wset);
	if (f->events & POLLPRI)
	  FD_SET (f->fd, &xset);
	if (f->fd > maxfd && (f->events & (POLLIN|POLLOUT|POLLPRI)))
	  maxfd = f->fd;
      }

  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  ready = select (maxfd + 1, &rset, &wset, &xset,
		  timeout == -1 ? NULL : &tv);
  if (ready > 0)
    for (f = fds; f < &fds[nfds]; ++f)
      {
	f->revents = 0;
	if (f->fd >= 0)
	  {
	    if (FD_ISSET (f->fd, &rset))
	      f->revents |= POLLIN;
	    if (FD_ISSET (f->fd, &wset))
	      f->revents |= POLLOUT;
	    if (FD_ISSET (f->fd, &xset))
	      f->revents |= POLLPRI;
	  }
      }

  return ready;
}

#endif /* !defined(HAVE_SYS_POLL_H) */

#if defined(_WIN32)

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#define  WIN32_LEAN_AND_MEAN
#include "glib.h"
#include <windows.h>
#include "poll.h"

typedef struct plst {
  HANDLE       pa, pb;
  HANDLE       ea, eb;
  int          cnt;
  struct plst *next;
} plst;

plst *pipelist=0;

void pipe_init()
{
}

static void addpipe(HANDLE pa, HANDLE pb, HANDLE ea, HANDLE eb)
{
  plst *n;

  n=g_new(plst, 1);
  n->pa=pa;
  n->pb=pb;
  n->ea=ea;
  n->eb=eb;
  n->cnt=0;
  n->next=pipelist;

  pipelist=n;
}

static void delpipe(HANDLE p)
{
  plst *t, *t2;

  if ((pipelist->pa==p) || (pipelist->pb==p))
  {
    g_free(pipelist);
    pipelist=0;
    return;
  }

  for (t=pipelist; t->next; t=t->next)
    if (((t2=t->next)->pa==p) || (t->next->pb==p))
    {
      t->next=t2->next;
      g_free(t2);
      return;
    }
}

static plst *findpipe(HANDLE p)
{
  plst *t;

  for (t=pipelist; t; t=t->next)
    if ((t->pa==p) || (t->pb==p))
      return t;

  return 0;
}

int pipe_open(int *bla)
{
  int r;
  HANDLE ea, eb;

  r=CreatePipe((PHANDLE) bla, (PHANDLE) bla+1, 0, 0);

  if (r)
  {
    ea=CreateEvent(0, 0, 1, 0);
    eb=CreateEvent(0, 0, 1, 0);
    addpipe((HANDLE) bla[0], (HANDLE) bla[1], ea, eb);
  }

  return r;
}

void pipe_close(int *bla)
{
  plst *a;

  if ((a=findpipe(bla[0])))
  {
    CloseHandle(a->pa);
    CloseHandle(a->pb);
    CloseHandle(a->ea);
    CloseHandle(a->eb);
  }
}

int pipe_write(int fd, void *buf, int cnt)
{
  DWORD written;
  plst *a;

  WriteFile((HANDLE) fd, buf, cnt, &written, 0);
  if ((a=findpipe((HANDLE) fd)))
  {
    SetEvent(a->ea);
    a->cnt+=cnt;

    if (!a->cnt)
    {
      ResetEvent(a->ea);
      ResetEvent(a->eb);
    }
  }

  return written;
}

int pipe_read(int fd, void *buf, int cnt)
{
  DWORD read;
  plst *a;

  ReadFile((HANDLE) fd, buf, cnt, &read, 0);
  if ((a=findpipe((HANDLE) fd)))
  {
    SetEvent(a->eb);
    a->cnt-=cnt;

    if (!a->cnt)
    {
      ResetEvent(a->eb);
    }
  }

  return read;
}

int
poll (fds, nfds, timeout)
     struct pollfd *fds;
     unsigned int nfds;
     int timeout;
{
  HANDLE handles[16];
  GPollFD *f;
  DWORD ready;
  MSG msg;
  UINT timer;
  LONG prevcnt;
  gint poll_msgs = -1;
  gint nhandles = 0;
  plst *a=0;

  for (f = fds; f < &fds[nfds]; ++f)
    if (f->fd >= 0)
      {
	if (f->events & G_IO_IN)
	  if (f->fd == G_WIN32_MSG_HANDLE)
	    poll_msgs = f - fds;
	  else
	    {
	      /* g_print ("g_poll: waiting for handle %#x\n", f->fd); */
        if ((a=findpipe((HANDLE) f->fd)))
        {
          if (a->cnt)
          {
            SetEvent(a->ea);
            SetEvent(a->eb);
          }

          handles[nhandles++]=a->ea;
          handles[nhandles++]=a->eb;
        }
        else
          handles[nhandles++] = (HANDLE) f->fd;
	    }
      }

  if (timeout == -1)
    timeout = INFINITE;

  if (poll_msgs >= 0)
    {
      /* Waiting for messages, and maybe events */
      if (nhandles == 0)
	{
	  if (timeout == INFINITE)
	    {
	      /* Waiting just for messages, infinite timeout
	       * -> Use PeekMessage, then WaitMessage
	       */
	      /* g_print ("WaitMessage, PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else if (!WaitMessage ())
		g_warning ("g_poll: WaitMessage failed");
	      ready = WAIT_OBJECT_0;
	    }
	  else if (timeout == 0)
	    {
	      /* Waiting just for messages, zero timeout
	       * -> Use PeekMessage
	       */
	      /* g_print ("PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else
		ready = WAIT_TIMEOUT;
	    }
	  else
	    {
	      /* Waiting just for messages, some timeout
	       * -> First try PeekMessage, then set a timer, wait for message,
	       * kill timer, use PeekMessage
	       */
	      /* g_print ("PeekMessage\n"); */
	      if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		ready = WAIT_OBJECT_0;
	      else if ((timer = SetTimer (NULL, 0, timeout, NULL)) == 0)
		g_warning ("g_poll: SetTimer failed");
	      else
		{
		  /* g_print ("WaitMessage\n"); */
		  WaitMessage ();
		  KillTimer (NULL, timer);
		  if (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		    ready = WAIT_OBJECT_0;
		  else
		    ready = WAIT_TIMEOUT;
		}
	    }
	}
      else
	{
	  /* Wait for either message or event
	   * -> Use MsgWaitForMultipleObjects
	   */
	  /* g_print ("MsgWaitForMultipleObjects(%d, %d)\n", nhandles, timeout); */
	  ready = MsgWaitForMultipleObjects (nhandles, handles, FALSE,
					     timeout, QS_ALLINPUT);
	  /* g_print("=%d\n", ready); */
	  if (ready == WAIT_FAILED)
      printf("poll: MsgWaitForMultipleObjects failed (%s)\n", g_win32_error_message(GetLastError()));
	}
    }
  else if (nhandles == 0)
    {
      /* Wait for nothing (huh?) */
      return 0;
    }
  else
    {
      /* Wait for just events
       * -> Use WaitForMultipleObjects
       */
      /* g_print ("WaitForMultipleObjects(%d, %d)\n", nhandles, timeout); */

      ready = WaitForMultipleObjects (nhandles, handles, FALSE, timeout);
/*    g_print("=%d\n", ready);*/
      if (ready == WAIT_FAILED)
        printf("poll: WaitForMultipleObjects failed (%s): %08x\n", g_win32_error_message(GetLastError()), handles[0]);
    }

  for (f = fds; f < &fds[nfds]; ++f)
    f->revents = 0;

  if (ready == WAIT_FAILED)
    return -1;
  else if (poll_msgs >= 0 && ready == WAIT_OBJECT_0 + nhandles)
    {
      fds[poll_msgs].revents |= G_IO_IN;
    }
  else if (ready >= WAIT_OBJECT_0 && ready < WAIT_OBJECT_0 + nhandles)
    for (f = fds; f < &fds[nfds]; ++f)
      {
        if (!(a=findpipe((HANDLE) f->fd)))
        {
          if ((f->events & POLLIN)
              && f->fd == (gint) handles[ready - WAIT_OBJECT_0])
            {
              f->revents |= G_IO_IN;
              /* g_print ("event %#x\n", f->fd); */
              ResetEvent((HANDLE) f->fd);
            }
        }
        else
        {
          if ((f->events & POLLIN)
              && (a->ea == handles[ready - WAIT_OBJECT_0]
              || a->eb == handles[ready - WAIT_OBJECT_0]))
            {
              f->revents |= G_IO_IN;
            }
        }
      }
    
  if (ready == WAIT_TIMEOUT)
    return 0;
  else
    return 1;
}

#endif /* defined(_WIN32) */
