/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2000Thomas Roessler <roessler@guug.de>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 *     02139, USA.
 */ 

/*
 * This file used to contain some more functions, namely those
 * which are now in muttlib.c.  They have been removed, so we have
 * some of our "standard" functions in external programs, too.
 */

#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib.h"

void mutt_nocurses_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
}

void *safe_calloc (size_t nmemb, size_t size)
{
  void *p;

  if (!nmemb || !size)
    return NULL;
  if (!(p = calloc (nmemb, size)))
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }
  return p;
}

void *safe_malloc (size_t siz)
{
  void *p;

  if (siz == 0)
    return 0;
  if ((p = (void *) malloc (siz)) == 0)
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }
  return (p);
}

void safe_realloc (void **p, size_t siz)
{
  void *r;

  if (siz == 0)
  {
    if (*p)
    {
      free (*p);
      *p = NULL;
    }
    return;
  }

  if (*p)
    r = (void *) realloc (*p, siz);
  else
  {
    /* realloc(NULL, nbytes) doesn't seem to work under SunOS 4.1.x */
    r = (void *) malloc (siz);
  }

  if (!r)
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }

  *p = r;
}

void safe_free (void **p)
{
  if (*p)
  {
    free (*p);
    *p = 0;
  }
}

int safe_fclose (FILE **f)
{
  int r = 0;
  
  if (*f)
    r = fclose (*f);
      
  *f = NULL;
  return r;
}

char *safe_strdup (const char *s)
{
  char *p;
  size_t l;

  if (!s || !*s)
    return 0;
  l = strlen (s) + 1;
  p = (char *)safe_malloc (l);
  memcpy (p, s, l);
  return (p);
}

void mutt_str_replace (char **p, const char *s)
{
  if (*p)
    free (*p);
  *p = safe_strdup (s);
}

/* convert all characters in the string to lowercase */
char *mutt_strlower (char *s)
{
  char *p = s;

  while (*p)
  {
    *p = tolower (*p);
    p++;
  }

  return (s);
}

void mutt_unlink (const char *s)
{
  FILE *f;
  struct stat sb;
  char buf[2048];
  
  if (stat (s, &sb) == 0)
  {
    if ((f = fopen (s, "r+")))
    {
      unlink (s);
      memset (buf, 0, sizeof (buf));
      while (sb.st_size > 0)
      {
	fwrite (buf, 1, sizeof (buf), f);
	sb.st_size -= sizeof (buf);
      }
      fclose (f);
    }
  }
}

int mutt_copy_bytes (FILE *in, FILE *out, size_t size)
{
  char buf[2048];
  size_t chunk;

  while (size > 0)
  {
    chunk = (size > sizeof (buf)) ? sizeof (buf) : size;
    if ((chunk = fread (buf, 1, chunk, in)) < 1)
      break;
    if (fwrite (buf, 1, chunk, out) != chunk)
    {
      /* dprint (1, (debugfile, "mutt_copy_bytes(): fwrite() returned short byte count\n")); */
      return (-1);
    }
    size -= chunk;
  }

  return 0;
}

int mutt_copy_stream (FILE *fin, FILE *fout)
{
  size_t l;
  char buf[LONG_STRING];

  while ((l = fread (buf, 1, sizeof (buf), fin)) > 0)
  {
    if (fwrite (buf, 1, l, fout) != l)
      return (-1);
  }

  return 0;
}

static int 
compare_stat (struct stat *osb, struct stat *nsb)
{
  if (osb->st_dev != nsb->st_dev || osb->st_ino != nsb->st_ino ||
      osb->st_rdev != nsb->st_rdev)
  {
    return -1;
  }

  return 0;
}

int safe_symlink(const char *oldpath, const char *newpath)
{
  struct stat osb, nsb;

  if(!oldpath || !newpath)
    return -1;
  
  if(unlink(newpath) == -1 && errno != ENOENT)
    return -1;
  
  if (oldpath[0] == '/')
  {
    if (symlink (oldpath, newpath) == -1)
      return -1;
  }
  else
  {
    char abs_oldpath[_POSIX_PATH_MAX];

    if ((getcwd (abs_oldpath, sizeof abs_oldpath) == NULL) ||
	(strlen (abs_oldpath) + 1 + strlen (oldpath) + 1 > sizeof abs_oldpath))
    return -1;
  
    strcat (abs_oldpath, "/");
    strcat (abs_oldpath, oldpath);
    if (symlink (abs_oldpath, newpath) == -1)
      return -1;
  }

  if(stat(oldpath, &osb) == -1 || stat(newpath, &nsb) == -1
     || compare_stat(&osb, &nsb) == -1)
  {
    unlink(newpath);
    return -1;
  }
  
  return 0;
}

/* 
 * This function is supposed to do nfs-safe renaming of files.
 * 
 * Warning: We don't check whether src and target are equal.
 */

int safe_rename (const char *src, const char *target)
{
  struct stat ssb, tsb;

  if (!src || !target)
    return -1;

  if (link (src, target) != 0)
  {
    return -1;
  }

  /*
   * Stat both links and check if they are equal.
   */
  
  if (stat (src, &ssb) == -1)
  {
    return -1;
  }
  
  if (stat (target, &tsb) == -1)
  {
    return -1;
  }

  /* 
   * pretend that the link failed because the target file
   * did already exist.
   */

  if (compare_stat (&ssb, &tsb) == -1)
  {
    errno = EEXIST;
    return -1;
  }

  /*
   * Unlink the original link.  Should we really ignore the return
   * value here? XXX
   */

  unlink (src);

  return 0;
}

int safe_open (const char *path, int flags)
{
  struct stat osb, nsb;
  int fd;

  if ((fd = open (path, flags, 0600)) < 0)
    return fd;

  /* make sure the file is not symlink */
  if (lstat (path, &osb) < 0 || fstat (fd, &nsb) < 0 ||
      compare_stat(&osb, &nsb) == -1)
  {
/*    dprint (1, (debugfile, "safe_open(): %s is a symlink!\n", path)); */
    close (fd);
    return (-1);
  }

  return (fd);
}

/* when opening files for writing, make sure the file doesn't already exist
 * to avoid race conditions.
 */
FILE *safe_fopen (const char *path, const char *mode)
{
  if (mode[0] == 'w')
  {
    int fd;
    int flags = O_CREAT | O_EXCL;

    if (mode[1] == '+')
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;

    if ((fd = safe_open (path, flags)) < 0)
      return (NULL);

    return (fdopen (fd, mode));
  }
  else
    return (fopen (path, mode));
}

static char safe_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%/";

void mutt_sanitize_filename (char *f, short slash)
{
  if (!f) return;

  for (; *f; f++)
  {
    if ((slash && *f == '/') || !strchr (safe_chars, *f))
      *f = '_';
  }
}

/* Read a line from ``fp'' into the dynamically allocated ``s'',
 * increasing ``s'' if necessary. The ending "\n" or "\r\n" is removed.
 * If a line ends with "\", this char and the linefeed is removed,
 * and the next line is read too.
 */
char *mutt_read_line (char *s, size_t *size, FILE *fp, int *line)
{
  size_t offset = 0;
  char *ch;

  if (!s)
  {
    s = safe_malloc (STRING);
    *size = STRING;
  }

  FOREVER
  {
    if (fgets (s + offset, *size - offset, fp) == NULL)
    {
      safe_free ((void **) &s);
      return NULL;
    }
    if ((ch = strchr (s + offset, '\n')) != NULL)
    {
      (*line)++;
      *ch = 0;
      if (ch > s && *(ch - 1) == '\r')
	*--ch = 0;
      if (ch == s || *(ch - 1) != '\\')
	return s;
      offset = ch - s - 1;
    }
    else if (feof (fp))
    {
      /* The last line of fp isn't \n terminated */
      (*line)++;
      return s;
    }
    else
    {
      /* There wasn't room for the line -- increase ``s'' */
      offset = *size - 1; /* overwrite the terminating 0 */
      *size += STRING;
      safe_realloc ((void **) &s, *size);
    }
  }
}

char *
mutt_substrcpy (char *dest, const char *beg, const char *end, size_t destlen)
{
  size_t len;

  len = end - beg;
  if (len > destlen - 1)
    len = destlen - 1;
  memcpy (dest, beg, len);
  dest[len] = 0;
  return dest;
}

char *mutt_substrdup (const char *begin, const char *end)
{
  size_t len;
  char *p;

  len = end - begin;
  p = safe_malloc (len + 1);
  memcpy (p, begin, len);
  p[len] = 0;
  return p;
}

/* prepare a file name to survive the shell's quoting rules.
 * From the Unix programming FAQ by way of Liviu.
 */

size_t mutt_quote_filename (char *d, size_t l, const char *f)
{
  size_t i, j = 0;

  if(!f) 
  {
    *d = '\0';
    return 0;
  }

  /* leave some space for the trailing characters. */
  l -= 6;
  
  d[j++] = '\'';
  
  for(i = 0; j < l && f[i]; i++)
  {
    if(f[i] == '\'' || f[i] == '`')
    {
      d[j++] = '\'';
      d[j++] = '\\';
      d[j++] = f[i];
      d[j++] = '\'';
    }
    else
      d[j++] = f[i];
  }
  
  d[j++] = '\'';
  d[j]   = '\0';
  
  return j;
}

/* NULL-pointer aware string comparison functions */

int mutt_strcmp(const char *a, const char *b)
{
  return strcmp(NONULL(a), NONULL(b));
}

int mutt_strcasecmp(const char *a, const char *b)
{
  return strcasecmp(NONULL(a), NONULL(b));
}

int mutt_strncmp(const char *a, const char *b, size_t l)
{
  return strncmp(NONULL(a), NONULL(b), l);
}

int mutt_strncasecmp(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l);
}

size_t mutt_strlen(const char *a)
{
  return a ? strlen (a) : 0;
}

const char *mutt_stristr (const char *haystack, const char *needle)
{
  const char *p, *q;

  if (!haystack)
    return NULL;
  if (!needle)
    return (haystack);

  while (*(p = haystack))
  {
    for (q = needle; *p && *q && tolower (*p) == tolower (*q); p++, q++)
      ;
    if (!*q)
      return (haystack);
    haystack++;
  }
  return NULL;
}

char *mutt_skip_whitespace (char *p)
{
  SKIPWS (p);
  return p;
}

void mutt_remove_trailing_ws (char *s)
{
  char *p;
  
  for (p = s + mutt_strlen (s) - 1 ; p >= s && ISSPACE (*p) ; p--)
    *p = 0;
}
