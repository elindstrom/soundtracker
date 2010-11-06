%define name soundtracker
%define version 0.6.8
%define release 30
%define prefix /usr

Summary: Sound modules editor/player

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Sound
Copyright: GPL

Url: http://www.soundtracker.org/

Source: ftp://ftp.soundtracker.org/pub/soundtracker/v0.6/soundtracker-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
SoundTracker is a pattern-oriented music editor for the X Window
System, similar to the DOS program 'FastTracker'. Samples are lined up
on tracks and patterns which are then arranged to a song. Supported
module formats are XM and MOD; the player code is the one from
OpenCP. A basic sample recorder and editor is also included.  The user
interface makes use of GTK+, and, optionally, GNOME.

%prep

%setup

%build
LINGUAS="da de es fr gl hr it ja no pl ru rw sk sl sv tr vi" CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --disable-alsa --disable-sndfile
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%doc ABOUT-NLS FAQ NEWS TODO README ChangeLog
%{prefix}/bin/soundtracker
%{prefix}/share/gnome/apps/Multimedia/soundtracker.desktop
%{prefix}/share/soundtracker/soundtracker_splash.png
%{prefix}/share/soundtracker/sharp.xpm
%{prefix}/share/soundtracker/flat.xpm
%{prefix}/share/soundtracker/rightarrow.xpm
%{prefix}/share/soundtracker/downarrow.xpm
%{prefix}/share/soundtracker/stop.xpm
%{prefix}/share/soundtracker/play.xpm
%{prefix}/share/soundtracker/play_cur.xpm
%{prefix}/share/soundtracker/lock.xpm
%{prefix}/share/soundtracker/muted.png
%{prefix}/share/locale/*/*/*

%clean
rm -r $RPM_BUILD_ROOT
