/*

Copyright (C) 2012 by Mark Heily <mark@heily.com>

 * portability.c - code to workaround the deficiencies of various platforms.
  
   * Copyright 2012 Rob Landley <rob@landley.net>
   * Copyright 2012 Georgi Chorbadzhiyski <gf@unixsol.org>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* NOTE: This is based on toybox compatibility.c 
         from http://www.landley.net/code/toybox/ 
 */

#if defined(__APPLE__) || defined(__ANDROID__)
ssize_t getdelim(char **linep, size_t *np, int delim, FILE *stream)
{
  int ch;
  size_t new_len;
  ssize_t i = 0;
  char *line, *new_line;

  // Invalid input
  if (!linep || !np) {
      errno = EINVAL;
      return -1;
  }

  if (*linep == NULL || *np == 0) {
      *np = 1024;
      *linep = calloc(1, *np);
      if (*linep == NULL)
          return -1;
  }
  line = *linep;

  while ((ch = getc(stream)) != EOF) {
      if ((unsigned int)i > *np) {
          // Need more space
          new_len  = *np + 1024;
          new_line = realloc(*linep, new_len);
          if (!new_line)
              return -1;
          *np    = new_len;
          *linep = new_line;
      }

      line[i] = ch;
      if (ch == delim)
          break;
      i += 1;
  }

  if ((unsigned int)i > *np) {
      // Need more space
      new_len  = i + 2;
      new_line = realloc(*linep, new_len);
      if (!new_line)
          return -1;
      *np    = new_len;
      *linep = new_line;
  }
  line[i + 1] = '\0';

  return i > 0 ? i : -1;
}

ssize_t getline(char **linep, size_t *np, FILE *stream) {
  return getdelim(linep, np, '\n', stream);
}
#endif
