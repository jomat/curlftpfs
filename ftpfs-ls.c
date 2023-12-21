/*
    FTP file system
    Copyright (C) 2006 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <ctype.h>
#include <strings.h>

#include "ftpfs.h"
#include "charset_utils.h"
#include "ftpfs-ls.h"

struct cache {
    int on;
    char incomplete[];
};

extern struct cache cache;

static int parse_dir_unix(const char *line,
                          struct stat *sbuf,
                          char *file,
                          char *link) {
  char mode[12];
  long nlink = 1;
  char user[33];
  char group[33];
  unsigned long long size;
  char month[4];
  char day[3];
  char year[6];
  char date[20];
  struct tm tm;
  time_t tt;
  int res;

  memset(file, 0, sizeof(char)*1024);
  memset(&tm, 0, sizeof(tm));
  memset(&tt, 0, sizeof(tt));

#define SPACES "%*[ \t]"
  res = sscanf(line,
               "%11s"
               "%lu"  SPACES
               "%32s" SPACES
               "%32s" SPACES
               "%llu" SPACES
               "%3s"  SPACES
               "%2s"  SPACES
               "%5s"  "%*c"
               "%1023c",
               mode, &nlink, user, group, &size, month, day, year, file);
  if (res < 9) {
    res = sscanf(line,
                 "%11s"
                 "%32s" SPACES
                 "%32s" SPACES
                 "%llu" SPACES
                 "%3s"  SPACES
                 "%2s"  SPACES
                 "%5s"  "%*c"
                 "%1023c",
                 mode, user, group, &size, month, day, year, file);
    if (res < 8) {
      return 0;
    }
  }
#undef SPACES

  char *link_marker = strstr(file, " -> ");
  if (link_marker) {
    strcpy(link, link_marker + 4);
    *link_marker = '\0';
  }

  int i = 0;
  if (mode[i] == 'd') {
    sbuf->st_mode |= S_IFDIR;
  } else if (mode[i] == 'l') {
    sbuf->st_mode |= S_IFLNK;
  } else {
    sbuf->st_mode |= S_IFREG;
  }
  for (i = 1; i < 10; ++i) {
    if (mode[i] != '-') {
      sbuf->st_mode |= 1 << (9 - i);
    }
  }

  sbuf->st_nlink = nlink;

  sbuf->st_size = size;
  if (ftpfs.blksize) {
    sbuf->st_blksize = ftpfs.blksize;
    sbuf->st_blocks =
      ((size + ftpfs.blksize - 1) & ~((unsigned long long) ftpfs.blksize - 1)) >> 9;
  }

  sprintf(date,"%s,%s,%s", year, month, day);
  tt = time(NULL);
  gmtime_r(&tt, &tm);
  tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
  if(strchr(year, ':')) {
    int cur_mon = tm.tm_mon;  // save current month
    strptime(date, "%H:%M,%b,%d", &tm);
    // Unix systems omit the year for the last six months
    if (cur_mon + 5 < tm.tm_mon) {  // month from last year
      DEBUG(2, "correct year: cur_mon: %d, file_mon: %d\n", cur_mon, tm.tm_mon);
      tm.tm_year--;  // correct the year
    }
  } else {
    strptime(date, "%Y,%b,%d", &tm);
  }

  sbuf->st_atime = sbuf->st_ctime = sbuf->st_mtime = mktime(&tm);

  return 1;
}

static int parse_dir_win(const char *line,
                         struct stat *sbuf,
                         char *file,
                         const char *link) {
  char date[9];
  char hour[8];
  char size[33];
  struct tm tm;
  time_t tt;
  int res;
  (void)link;

  memset(file, 0, sizeof(char)*1024);
  memset(&tm, 0, sizeof(tm));
  memset(&tt, 0, sizeof(tt));

  res = sscanf(line, "%8s%*[ \t]%7s%*[ \t]%32s%*[ \t]%1023c",
               date, hour, size, file);
  if (res < 4) {
    return 0;
  }

  DEBUG(2, "date: %s hour: %s size: %s file: %s\n", date, hour, size, file);

  tt = time(NULL);
  gmtime_r(&tt, &tm);
  tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
  strptime(date, "%m-%d-%y", &tm);
  strptime(hour, "%I:%M%p", &tm);

  sbuf->st_atime = sbuf->st_ctime = sbuf->st_mtime = mktime(&tm);

  sbuf->st_nlink = 1;

  if (!strcmp(size, "<DIR>")) {
    sbuf->st_mode |= S_IFDIR;
  } else {
    unsigned long long nsize = strtoull(size, NULL, 0);
    sbuf->st_mode |= S_IFREG;
    sbuf->st_size = nsize;
    if (ftpfs.blksize) {
      sbuf->st_blksize = ftpfs.blksize;
      sbuf->st_blocks =
        ((nsize + ftpfs.blksize - 1) & ~((unsigned long long) ftpfs.blksize - 1)) >> 9;
    }
  }

  return 1;
}

static int parse_dir_netware(const char *line,
                             struct stat *sbuf,
                             const char *file,
                             const char *link) {
  (void) line;
  (void) sbuf;
  (void) file;
  (void) link;
  return 0;
}

static int parse_dir_apache(const char *line,
			    struct stat *sbuf,
			    char *file,
			    char *link) {
  long nlink = 1;
  long size;
  int year = -1, month = -1, day = -1, hr = -1, min = -1;
  char monthstr[10] = "", sizestr[100] = "";
  int sizemultval, len;
  struct tm tm;
  time_t tt;
  int res;
  char monthabbrev[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  char *p;
  char linecopy[1024], file2[1024];
  int quotemode = 0;
  int sizeint = 0, sizetenth = 0;
  char sizemultstr[10];

  memset(linecopy, 0, sizeof(linecopy));
  strcpy(linecopy, line);
  memset(file,  0, sizeof(char)*1024);
  memset(file2, 0, sizeof(file2));
  *link = 0;

  /* Convert markup to upper case */
  for (p=linecopy; *p; p++) {
    if (*p == '"' || *p == '\'') quotemode = 1-quotemode;
    if (!quotemode) *p = toupper(*p);
  }

  /* Skip to <A HREF=" */
  p = strstr(linecopy, "<A ");
  if (p == 0) return 0;
  res = sscanf(p, "<A%*[^H]HREF=\"%[^\"<>]%*[^>]>%[^<]</A> %2d-%[^-]-%4d %d:%d %s",
	       file, file2, &day, monthstr, &year, &hr, &min, sizestr);
  if (res < 8) return 0;
  /* Check that the two file names are the same, at least the first
     few characters.  This is just a heuristic to get rid of junk. */
  if (strncasecmp(file, file2,10) != 0) return 0;

  size = 0;
  sizemultval = 1;
  if (sscanf(sizestr, "%d.%d%[kKmMgG]", &sizeint, &sizetenth, sizemultstr) == 3) {
    if (toupper(sizemultstr[0]) == 'K') sizemultval = 1024;
    if (toupper(sizemultstr[0]) == 'M') sizemultval = 1024*1024;
    if (toupper(sizemultstr[0]) == 'G') sizemultval = 1024*1024*1024;
    if (sizeint > 0 || sizetenth > 0) {
      size = sizeint*sizemultval + (sizetenth)*sizemultval/10;
    }
  } else if (sscanf(sizestr, "%d%[kKmMgG]", &sizeint, sizemultstr) == 2) {
    if (toupper(sizemultstr[0]) == 'K') sizemultval = 1024;
    if (toupper(sizemultstr[0]) == 'M') sizemultval = 1024*1024;
    if (toupper(sizemultstr[0]) == 'G') sizemultval = 1024*1024*1024;
    if (sizeint > 0) {
      size = (sizeint)*sizemultval;
    }
  } else if (sscanf(sizestr, "%ld", &size) == 1) {
    /* Dummy */
  }

  memset(&tm, 0, sizeof(tm));
  memset(&tt, 0, sizeof(tt));

  len = strlen(file);
  if (file[len-1] == '/') {
    sbuf->st_mode |= S_IFDIR;
    sbuf->st_mode |= 0111;    /* --x--x--x */
    file[len-1] = 0;
  } else {
    sbuf->st_mode |= S_IFREG;
  }
  sbuf->st_mode |= 0444;      /* r--r--r-- */

  sbuf->st_nlink = nlink;
  sbuf->st_size = size;

  /* Find the correct month number.  Note that strptime is not
     necessarily useful for locale-specific dates, since the server
     and the client may be in different locales.  Assume that the
     month is in English. */
  for (month=0; month<12; month++) {
    if (strcasecmp(monthstr, monthabbrev[month]) == 0) break;
  }
  if (month == 13) month = 0;

  tm.tm_sec = 0;
  tm.tm_min = min;
  tm.tm_hour = hr;
  tm.tm_mday = day;
  tm.tm_mon = month;
  tm.tm_year = year - 1900;

  sbuf->st_atime = sbuf->st_ctime = sbuf->st_mtime = mktime(&tm);

  return 1;

}

int parse_dir(const char* list, const char* dir,
              const char* name, struct stat* sbuf,
              char* linkbuf, int linklen,
              fuse_cache_dirh_t h, fuse_cache_dirfil_t filler) {
  char *file;
  char *link;
  const char *start = list;
  const char *end = list;
  char found = 0;
  struct stat stat_buf;

  if (sbuf) memset(sbuf, 0, sizeof(struct stat));

  if (name && sbuf && name[0] == '\0') {
    sbuf->st_mode |= S_IFDIR;
    sbuf->st_mode |= 0755;
    sbuf->st_size = 1024;
    sbuf->st_nlink = 1;
    return 0;
  }

  file = (char *)malloc(1024*sizeof(char));
  link = (char *)malloc(1024*sizeof(char));

  while ((end = strchr(start, '\n')) != NULL) {
    char* line;

    memset(&stat_buf, 0, sizeof(stat_buf));

    if (end > start && *(end-1) == '\r') end--;

    line = (char*)malloc(end - start + 1);
    strncpy(line, start, end - start);
    line[end - start] = '\0';
    start = *end == '\r' ? end + 2 : end + 1;

    if (ftpfs.codepage) {
      convert_charsets(ftpfs.codepage, ftpfs.iocharset, &line);
    }

    file[0] = link[0] = '\0';
    int res;
    if (!ftpfs.is_http) {
      res = parse_dir_unix(line, &stat_buf, file, link) ||
            parse_dir_win(line, &stat_buf, file, link) ||
            parse_dir_netware(line, &stat_buf, file, link);
    } else {
      res = parse_dir_apache(line, &stat_buf, file, link);
    }

    if (res) {
      char *full_path = g_strdup_printf("%s%s", dir, file);

      if (link[0]) {
        char *reallink;
        if (link[0] == '/' && ftpfs.symlink_prefix_len) {
          reallink = g_strdup_printf("%s%s", ftpfs.symlink_prefix, link);
        } else {
          reallink = g_strdup(link);
        }
        int linksize = strlen(reallink);
        if (cache.on) {
          cache_add_link(full_path, reallink, linksize+1);
          DEBUG(1, "cache_add_link: %s %s\n", full_path, reallink);
        }
        if (linkbuf && linklen) {
          if (linksize > linklen) linksize = linklen - 1;
          strncpy(linkbuf, reallink, linksize);
          linkbuf[linksize] = '\0';
        }
        free(reallink);
      }

      if (h && filler) {
        DEBUG(1, "filler: %s\n", file);
        filler(h, file, &stat_buf);
      } else {
        if (cache.on) {
          DEBUG(1, "cache_add_attr: %s\n", full_path);
          cache_add_attr(full_path, &stat_buf);
        }
      }

      DEBUG(2, "comparing %s %s\n", name, file);
      if (name && !strcmp(name, file)) {
        if (sbuf) *sbuf = stat_buf;
        found = 1;
      }

      free(full_path);
    }

    free(line);
  }

  free(file);
  free(link);

  return !found;
}
