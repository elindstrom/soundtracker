
#ifndef _ST_I18N_H
#define _ST_I18N_H

#include <config.h>

#if defined(USE_GNOME)

#include <gnome.h>

#else

#ifndef _
#if defined(ENABLE_NLS)
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#  define gettext(x) (x)
#endif
#endif

#endif /* USE_GNOME */

#endif /* _ST_I18N_H */
