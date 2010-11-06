
/*
 * The Real SoundTracker - poll() emulation using select()
 *
 * Copyright (C) 1994, 1996 Free Software Foundation, Inc.
 * Copyright (C) 1998 Michael Krause
 * Part of this file was part of the GNU C Library.
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

#ifndef _ST_POLL_H
#define _ST_POLL_H

#include <config.h>

#ifdef HAVE_POLL
#include <sys/poll.h>
#else

/* Data structure describing a polling request.  */
struct pollfd
  {
    int fd;			/* File descriptor to poll.  */
    short int events;		/* Types of events poller cares about.  */
    short int revents;		/* Types of events that actually occurred.  */
  };

/* Event types that can be polled for.  These bits may be set in `events'
   to indicate the interesting event types; they will appear in `revents'
   to indicate the status of the file descriptor.  */
#define POLLIN		01              /* There is data to read.  */
#define POLLPRI		02              /* There is urgent data to read.  */
#define POLLOUT		04              /* Writing now will not block.  */

int poll(struct pollfd *ufds, unsigned int nfds, int timeout);

#endif /* HAVE_POLL */

#if defined(_WIN32)

void pipe_init();
int pipe_open();
void pipe_close();
int pipe_write(int fd, void *buf, int cnt);
int pipe_read(int fd, void *buf, int cnt);

#endif /* defined(_WIN32) */

#endif /* _ST_POLL_H */
