
/*
 * The Real SoundTracker - dsound (output) driver.
 *
 * Copyright (C) 1999 Tammo Hinrichs
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

#if defined(_WIN32)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "driver-inout.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"
#include "winpipe.h"

#include <windows.h>
#include <windowsx.h>
#include <dsound.h>

typedef HRESULT (WINAPI *dscfunc)(LPGUID,LPDIRECTSOUND*,LPUNKNOWN);

typedef struct {
    GtkWidget *configwidget;

    int out_sock, out_bits, out_channels, out_rate;
    int samples_per_frame;
    int mf;

    void *buf;
    int fakepipe[2];
    gpointer polltag;

    long buflen;
    signed long playpos, lastpos;

    HINSTANCE hdsinst;
    HWND winhwnd;
    HANDLE thread;
    dscfunc DSCreate;

    LPDIRECTSOUND lpds;
    DSCAPS dscaps;

    WAVEFORMATEX pbformat, sbformat;
    WAVEFORMATEX *wbformat;
    DSBUFFERDESC pbdesc, sbdesc;
    LPDIRECTSOUNDBUFFER pbuffer, sbuffer, wbuffer;
    DSBCAPS pbcaps;

    int  locked;
    void *lockbuf,*lockbuf2;
    DWORD locklen,locklen2;

    char *tempbuf;
    int writeprim;
    int sampshift;

    double outtime;
    double playtime;
} dsound_driver;

static int thread;

static int
dsound_get_buffer_pos (void *dp)
{
    dsound_driver *d=dp;
    int pc;

    IDirectSoundBuffer_GetCurrentPosition(d->wbuffer,(DWORD*) &pc,0);
    return pc;
}

static void
dsound_poll_ready_playing (gpointer data,
			gint source,
			GdkInputCondition condition)
{
    dsound_driver * const d = data;
    int bufpos, delta, pos, lockok=0, len1, len2;
    char bla;

    bufpos=dsound_get_buffer_pos(data);
    delta=((bufpos>=d->lastpos)?0:d->buflen)+bufpos-d->lastpos;

    pos=(bufpos+delta)%d->buflen;
    d->playpos+=delta;

    while (!lockok)
    {
      switch (IDirectSoundBuffer_Lock(d->wbuffer,d->lastpos,delta,&d->lockbuf,&d->locklen,&d->lockbuf2,&d->locklen2,0))
      {
        case DS_OK:
          lockok=1;
          break;
        case DSERR_BUFFERLOST:
          if FAILED(IDirectSoundBuffer_Restore(d->wbuffer))
            lockok=2;
          break;
        default:
          lockok=2;
      }
    }

    if (lockok==1)
    {
      audio_mix(d->lockbuf,d->locklen>>d->sampshift,d->out_rate,d->mf);
      if (d->locklen2)
        audio_mix(d->lockbuf2,d->locklen2>>d->sampshift,d->out_rate,d->mf);
    }

    IDirectSoundBuffer_Unlock(d->wbuffer,d->lockbuf,d->locklen,d->lockbuf2,d->locklen2);
    d->lastpos=bufpos;
}

static DWORD WINAPI
dsound_thread(LPVOID dp)
{
  dsound_driver * const d = dp;

  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

  while (thread)
  {
    dsound_poll_ready_playing(d, 0, 0);
    Sleep(25); /* sonst zieht das entschieden zuviel rechenzeit */
  }

  return 0;
}

static void
dsound_make_config_widgets (dsound_driver *d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
   
    thing = gtk_label_new(_("in beta state. aber ryg rockt."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget *
dsound_getwidget (void *dp)
{
    dsound_driver * const d = dp;

    return d->configwidget;
}

static void *
dsound_new (void)
{
    dsound_driver *d = g_new(dsound_driver, 1);

    d->out_bits = 16;
    d->out_rate = 44100;
    d->out_channels = 2;
    d->pbuffer = 0;
    d->sbuffer = 0;
    d->wbuffer = 0;
    d->writeprim = 0;

    dsound_make_config_widgets(d);

    return d;
}

static void
dsound_destroy (void *dp)
{
    dsound_driver * const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_free(dp);
}

static void
dsound_release (void *dp)
{
    dsound_driver * const d = dp;

    thread=0;
    WaitForSingleObject(d->thread, 1000);

    if (d->sbuffer)
    {
      IDirectSoundBuffer_Stop(d->sbuffer);
      IDirectSoundBuffer_Release(d->sbuffer);
    }

    if (d->tempbuf)
      g_free(d->tempbuf);

    d->sbuffer=0;
    d->tempbuf=0;

    IDirectSound_Release(d->lpds);
}

static gboolean
dsound_play (void *dp)
{
    dsound_driver *d = dp;
    int len;

    memset(&d->pbformat,0,sizeof(WAVEFORMATEX));
    d->pbformat.wFormatTag=WAVE_FORMAT_PCM;
    d->pbformat.nChannels=d->out_channels;
    d->pbformat.nSamplesPerSec=d->out_rate;
    d->pbformat.wBitsPerSample=d->out_bits;
    d->pbformat.nBlockAlign=d->pbformat.wBitsPerSample/8*d->pbformat.nChannels;
    d->pbformat.nAvgBytesPerSec=d->pbformat.nSamplesPerSec*d->pbformat.nBlockAlign;
    if FAILED(IDirectSoundBuffer_SetFormat(d->pbuffer,&d->pbformat))
    {
      if (d->writeprim)
      {
        IDirectSound_SetCooperativeLevel(d->lpds,d->winhwnd,DSSCL_PRIORITY);
        d->writeprim=0;
      }
    }

    IDirectSoundBuffer_GetFormat(d->pbuffer,&d->pbformat,sizeof(d->pbformat),0);

    len=(d->out_bits/8*d->out_channels*d->out_rate)/5; // 200ms buf default

    if (d->writeprim)
    {
      d->pbcaps.dwSize=sizeof(d->pbcaps);
      IDirectSoundBuffer_GetCaps(d->pbuffer,&d->pbcaps);
      len=d->pbcaps.dwBufferBytes;
      d->wbuffer=d->pbuffer;
      d->wbformat=&d->pbformat;
    }
    else
    {
      len=(len<DSBSIZE_MIN)?DSBSIZE_MIN:(len>DSBSIZE_MAX)?DSBSIZE_MAX:len;

      memset(&d->sbformat,0,sizeof(WAVEFORMATEX));
      d->sbformat.wFormatTag=WAVE_FORMAT_PCM;
      d->sbformat.nChannels=d->out_channels;
      d->sbformat.nSamplesPerSec=d->out_rate;
      d->sbformat.wBitsPerSample=d->out_bits;
      d->sbformat.nBlockAlign=d->sbformat.wBitsPerSample/8*d->sbformat.nChannels;
      d->sbformat.nAvgBytesPerSec=d->sbformat.nSamplesPerSec*d->sbformat.nBlockAlign;

      memset(&d->sbdesc,0,sizeof(DSBUFFERDESC));
      d->sbdesc.dwSize=sizeof(DSBUFFERDESC);
      d->sbdesc.dwFlags=DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS|DSBCAPS_STICKYFOCUS;
      d->sbdesc.dwBufferBytes=len;
      d->sbdesc.lpwfxFormat=&d->sbformat;

      if FAILED(IDirectSound_CreateSoundBuffer(d->lpds,&d->sbdesc,&d->sbuffer,0))
        return FALSE;

      d->wbuffer=d->sbuffer;
      d->wbformat=&d->sbformat;
    }

    d->buflen=len;

    if FAILED(IDirectSoundBuffer_Lock(d->wbuffer,0,0,&d->lockbuf,&d->locklen,0,0,DSBLOCK_ENTIREBUFFER))
    {
      IDirectSoundBuffer_Unlock(d->wbuffer,d->lockbuf,0,0,0);

      if (d->writeprim)
      {
        d->writeprim=0;
        IDirectSound_SetCooperativeLevel(d->lpds,d->winhwnd,DSSCL_PRIORITY);
        return dsound_play(dp);
      }
      else
      {
        IDirectSoundBuffer_Release(d->sbuffer);
        return FALSE;
      }
    }
    else
    {
      memset(d->lockbuf,0,d->locklen);
      IDirectSoundBuffer_Unlock(d->wbuffer,d->lockbuf,0,0,0);
    }

    if FAILED(IDirectSoundBuffer_Play(d->wbuffer,0,0,DSBPLAY_LOOPING))
    {
      if (d->writeprim)
      {
        d->writeprim=0;
        IDirectSound_SetCooperativeLevel(d->lpds,d->winhwnd,DSSCL_PRIORITY);
        return dsound_play(dp);
      }
      else
      {
        IDirectSoundBuffer_Release(d->sbuffer);
        return FALSE;
      }
    }

    d->tempbuf=g_new(char, len);
    memset(d->tempbuf,0,len);
    d->buf=d->tempbuf;

    d->locked=0;
    d->playpos=-d->buflen;
    d->lastpos=0;

    return TRUE;
}

static gboolean
dsound_open (void *dp)
{
    dsound_driver * const d = dp;
    int out_format;
    DWORD tid;
    char bla;

    d->winhwnd=GetForegroundWindow();
    d->writeprim=0;

    d->hdsinst=LoadLibrary("dsound.dll");
    if (!d->hdsinst)
      return FALSE;

    d->DSCreate=(dscfunc)GetProcAddress(d->hdsinst,"DirectSoundCreate");
    if (!d->DSCreate)
      return FALSE;

    if FAILED(d->DSCreate(0,&d->lpds,0))
      return FALSE;

    d->dscaps.dwSize=sizeof(DSCAPS);
    if FAILED(IDirectSound_GetCaps(d->lpds,&d->dscaps))
      return FALSE;

    if FAILED(IDirectSound_SetCooperativeLevel(d->lpds,d->winhwnd,d->writeprim?DSSCL_WRITEPRIMARY:DSSCL_PRIORITY))
    {
      d->writeprim=0;
      if FAILED(IDirectSound_SetCooperativeLevel(d->lpds,d->winhwnd,DSSCL_PRIORITY))
        return FALSE;
    }

    memset(&d->pbdesc,0,sizeof(DSBUFFERDESC));
    d->pbdesc.dwSize=sizeof(DSBUFFERDESC);
    d->pbdesc.dwFlags=DSBCAPS_PRIMARYBUFFER;
    if FAILED(IDirectSound_CreateSoundBuffer(d->lpds,&d->pbdesc,&d->pbuffer,0))
      return FALSE;

    if (!dsound_play(dp))
      return FALSE;

    d->sampshift=(d->out_bits==16)?1:0;
    d->sampshift+=(d->out_channels==2)?1:0;

    d->mf=(d->out_bits==16)?ST_MIXER_FORMAT_S16_LE:ST_MIXER_FORMAT_U8;
    d->mf|=(d->out_channels==2)?ST_MIXER_FORMAT_STEREO:0;
//  d->polltag=audio_poll_add(d->fakepipe[0], GDK_INPUT_READ, dsound_poll_ready_playing, d);

    /* now kick the "polling" (which sucks coz of pipe-faking) */

    thread=1;
    d->thread=CreateThread(0, 8*1024, dsound_thread, d, 0, &tid);

    return TRUE;
}

static double
dsound_get_play_time (void *dp)
{
    dsound_driver * const d = dp;

    return (double) d->playpos/(double) d->wbformat->nAvgBytesPerSec;
}

static gboolean
dsound_loadsettings (void *dp,
		  prefs_node *f)
{
//    dsound_driver * const d = dp;

    return TRUE;
}

static gboolean
dsound_savesettings (void *dp,
		  prefs_node *f)
{
//    dsound_driver * const d = dp;

    return TRUE;
}

st_io_driver driver_out_dsound = {
    { "DirectSound Output",

      dsound_new,
      dsound_destroy,

      dsound_open,
      dsound_release,

      dsound_getwidget,
      dsound_loadsettings,
      dsound_savesettings,
    },

    dsound_get_play_time,
};

#endif /* defined(_WIN32) */
