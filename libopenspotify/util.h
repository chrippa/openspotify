/*
 * $Id: util.h 326 2009-05-29 23:32:36Z dstien $
 *
 */

#ifndef DESPOTIFY_UTIL_H
#define DESPOTIFY_UTIL_H

#ifdef _MSC_VER
#include <basetsd.h>
#define ssize_t SSIZE_T
#else
#include <unistd.h>
#endif

#define DSFYfree(p) do { free(p); (p) = NULL; } while (0)
#define DSFYstrncat(target, data, size) do { strncat(target, data, size-1); ((unsigned char*)target)[size-1] = 0; } while (0)
#define DSFYstrncpy(target, data, size) do { strncpy(target, data, size-1); ((unsigned char*)target)[size-1] = 0; } while (0)

unsigned char *hex_ascii_to_bytes (char *, unsigned char *, int);
char *hex_bytes_to_ascii (unsigned char *, char *, int);
void hexdump8x32 (char *, void *, int);
void fhexdump8x32 (FILE *, char *, void *, int);
void logdata (char *, int, void *, int);
int block_read (int, void *, int);
int block_write (int, void *, int);
#endif
