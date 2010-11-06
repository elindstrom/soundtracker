
/*
 * The Real SoundTracker - Preferences handling
 *
 * Copyright (C) 1998-2001 Michael Krause
 * Copyright (C) 2000 Fabian Giesen
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

#if !defined(_WIN32)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include <glib.h>

#include "i18n.h"
#include "gui-subs.h"
#include "preferences.h"
#include "menubar.h"
#include "scope-group.h"
#include "track-editor.h"
#include "errors.h"

struct prefs_node {
    gboolean writemode;
    FILE *file;
};

char *
prefs_get_prefsdir (void)
{
    static char xdir[128];
    struct passwd *pw;

    pw = getpwuid(getuid());
    sprintf(xdir, "%s/.soundtracker2", pw->pw_dir);
    return(xdir);
}

static void
prefs_check_prefs_dir (void)
{
    struct stat st;
    char *dir = prefs_get_prefsdir();

    if(stat(dir, &st) < 0) {
	mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
    strcat(dir, "/tmp");
	mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
	error_warning(_("A directory called '.soundtracker2' has been created in your\nhome directory to store configuration files.\n"));
    }
}

const char *
prefs_get_filename (const char *name)
{
    static char buf[256];

    prefs_check_prefs_dir();
    sprintf(buf, "%s/%s", prefs_get_prefsdir(), name);
    return buf;
}

prefs_node *
prefs_open_read (const char *name)
{
    prefs_node *p = g_new(prefs_node, 1);
    char buf[256];

    if(p) {
	p->writemode = FALSE;
	sprintf(buf, "%s/%s", prefs_get_prefsdir(), name);
	p->file = fopen(buf, "rb");
	if(p->file) {
	    return p;
	}
	g_free(p);
    }

    return NULL;
}

prefs_node *
prefs_open_write (const char *name)
{
    prefs_node *p = g_new(prefs_node, 1);
    char buf[256];

    if(p) {
	p->writemode = TRUE;
	prefs_check_prefs_dir();
	sprintf(buf, "%s/%s", prefs_get_prefsdir(), name);
	p->file = fopen(buf, "wb");
	if(p->file) {
	    return p;
	}
	g_free(p);
    }

    return NULL;
}

void
prefs_close (prefs_node *f)
{
    if(f) {
	fclose(f->file);
	g_free(f);
    }
}

FILE *
prefs_get_file_pointer (prefs_node *p)
{
    if(p) {
	return p->file;
    } else {
	return NULL;
    }
}

static gboolean
prefs_get_line (FILE *f,
		const char *key,
		char *destbuf,
		int destbuflen)
{
    int i;
    char readbuf[1024], *p;

    for(i = 0; i < 2; i++) {
	// Linear search
	while(!feof(f)) {
	    fgets(readbuf, 1024, f);
	    p = strchr(readbuf, '=');
	    if(!p || p == readbuf || p[1] == 0) {
		return 0;
	    }
	    p[-1] = 0;
	    if(!strcmp(readbuf, key)) {
		strncpy(destbuf, p + 2, destbuflen - 1);
		destbuf[destbuflen - 1] = 0;

		if(strlen(destbuf) > 0 && destbuf[strlen(destbuf) - 1] == '\n')
		    destbuf[strlen(destbuf) - 1] = 0;

		return 1;
	    }
	}

	// Start from the beginning
	fseek(f, 0, SEEK_SET);
    }

    return 0;
}

gboolean
prefs_get_int (prefs_node *pn,
	       const char *key,
	       int *dest)
{
    char buf[21];
    FILE *f;

    if(pn->writemode)
	return FALSE;
    f = pn->file;

    if(prefs_get_line(f, key, buf, 20)) {
	buf[20] = 0;
	*dest = atoi(buf);
	return 1;
    }

    return 0;
}

gboolean
prefs_get_string (prefs_node *pn,
		  const char *key,
		  char *dest)
{
    char buf[128];
    FILE *f;

    if(pn->writemode)
	return FALSE;
    f = pn->file;

    if(prefs_get_line(f, key, buf, 127)) {
	buf[127] = 0;
	strcpy(dest, buf);
	return 1;
    }

    return 0;
}

void
prefs_put_int (prefs_node *pn,
	       const char *key,
	       int value)
{
    FILE *f;

    if(!pn->writemode)
	return;
    f = pn->file;

    fprintf(f, "%s = %d\n", key, value);
}

void
prefs_put_string (prefs_node *pn,
		  const char *key,
		  const char *value)
{
    FILE *f;

    if(!pn->writemode)
	return;
    f = pn->file;

    fprintf(f, "%s = %s\n", key, value);
}

#else /* defined(_WIN32) */

/*

  Attention!

  These don't work in their current form; what needs to be done is:

  a, change void* into prefs_node*
  b, add prefs_get_file_pointer() - if this is used by the caller
     after opening, use a real file instead of the registry.

 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "i18n.h"
#include "gui-subs.h"
#include "preferences.h"
#include "menubar.h"
#include "scope-group.h"
#include "track-editor.h"
#include "errors.h"

void *
prefs_open_read (const char *name)
{
    char buf[256];
    HKEY hk;

    sprintf(buf, "Software/Soundtracker/Soundtracker/%s", name);

    while(strchr(buf, '/'))
        *strchr(buf, '/')='\\';

    if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_ALL_ACCESS, &hk))
        hk=0;

    return (void *) hk;
}

void *
prefs_open_write (const char *name)
{
    char buf[256];
    HKEY hk;
    DWORD bla;

    sprintf(buf, "Software/Soundtracker/Soundtracker/%s", name);

    while(strchr(buf, '/'))
        *strchr(buf, '/')='\\';

    if (!RegCreateKeyEx(HKEY_LOCAL_MACHINE, buf, 0, 0, 0, KEY_ALL_ACCESS, 0, &hk, &bla))
      hk=0;

    return (void *) hk;
}

void
prefs_close (void *node)
{
    HKEY hk;

    hk=(HKEY) node;

    RegFlushKey(hk);
    RegCloseKey(hk);
}

static int
prefs_query_reg (HKEY hk,
           const char *key,
           char **buf,
           DWORD *size,
           DWORD *type)
{
    if(RegQueryValueEx(hk, key, 0, type, 0, size)==ERROR_SUCCESS)
    {
        *buf=(char *) malloc(*size+1);

        if(RegQueryValueEx(hk, key, 0, type, *buf, size)==ERROR_SUCCESS)
            return 1;
        else
        {
            free(*buf);
            return 0;
        }
    }
    else
        return 0;
}

static void
prefs_set_reg (HKEY hk,
           const char *key,
           char *buf,
           DWORD size,
           DWORD type)
{
    RegSetValueEx(hk, key, 0, type, buf, size);
}

gboolean
prefs_get_int (void *f,
	       const char *key,
	       int *dest)
{
    char *buf;
    DWORD sz, type;
    HKEY hk;

    hk=(HKEY) f;

    if(prefs_query_reg(hk, key, &buf, &sz, &type))
    {
        if (type==REG_DWORD)
        {
            *dest=*((DWORD *) buf);
            free(buf);
            return 1;
        }
    }

    return 0;
}

gboolean
prefs_get_string (void *f,
		  const char *key,
		  char *dest)
{
    char *buf;
    DWORD sz, type;
    HKEY hk;

    hk=(HKEY) f;

    if(prefs_query_reg(hk, key, &buf, &sz, &type))
    {
        if (type==REG_SZ)
        {
            buf[127]=0;
            strcpy(dest, buf);
            free(buf);
            return 1;
        }
    }

    return 0;
}

void
prefs_put_int (void *f,
	       const char *key,
	       int value)
{
    prefs_set_reg((HKEY) f, key, &value, 4, REG_DWORD);
}

void
prefs_put_string (void *f,
		  const char *key,
		  const char *value)
{
    prefs_set_reg((HKEY) f, key, value, strlen(value+1), REG_SZ);
}

#endif /* defined(_WIN32) */
