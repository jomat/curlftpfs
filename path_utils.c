/*
    FTP file system
    Copyright (C) 2007 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "path_utils.h"
#include "charset_utils.h"
#include "ftpfs.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str) {
  char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~'
      || *pstr == ':' || *pstr == '/')
      *pbuf++ = *pstr;
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

char* get_file_name(const char* path) {
  const char* filename = strrchr(path, '/');
  if (filename == NULL) filename = path;
  else ++filename;

  char* ret = strdup(filename);
  if (ftpfs.codepage) {
    convert_charsets(ftpfs.iocharset, ftpfs.codepage, &ret);
  }
  
  ret=url_encode(ret);
  return ret;
}

char* get_full_path(const char* path) {
  char* ret;
  char* converted_path = NULL;
  
  ++path;

  if (ftpfs.codepage && strlen(path)) {
    converted_path = strdup(path);
    convert_charsets(ftpfs.iocharset, ftpfs.codepage, &converted_path);
    path = converted_path;
  }

  ret = g_strdup_printf("%s%s", ftpfs.host, path);

  free(converted_path);

  ret=url_encode(ret);
  return ret;
}

char* get_fulldir_path(const char* path) {
  char* ret;
  char* converted_path = NULL;

  ++path;

  if (ftpfs.codepage && strlen(path)) {
    converted_path = strdup(path);
    convert_charsets(ftpfs.iocharset, ftpfs.codepage, &converted_path);
    path = converted_path;
  }

  ret = g_strdup_printf("%s%s%s", ftpfs.host, path, strlen(path) ? "/" : "");

  free(converted_path);

  ret=url_encode(ret);
  return ret;
}

char* get_dir_path(const char* path) {
  char* ret;
  char* converted_path = NULL;
  const char *lastdir;

  ++path;
  
  lastdir = strrchr(path, '/');
  if (lastdir == NULL) lastdir = path;

  if (ftpfs.codepage && (lastdir - path > 0)) {
    converted_path = g_strndup(path, lastdir - path);
    convert_charsets(ftpfs.iocharset, ftpfs.codepage, &converted_path);
    path = converted_path;
    lastdir = path + strlen(path);
  }

  ret = g_strdup_printf("%s%.*s%s",
                        ftpfs.host,
                        lastdir - path,
                        path,
                        lastdir - path ? "/" : "");

  free(converted_path);

  ret=url_encode(ret);
  return ret;
}
