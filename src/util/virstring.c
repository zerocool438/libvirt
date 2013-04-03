/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Daniel P. Berrange <berrange@redhat.com>
 */

#include <config.h>

#include "virstring.h"
#include "memory.h"
#include "buf.h"
#include "virterror_internal.h"

#define VIR_FROM_THIS VIR_FROM_NONE

/*
 * The following virStringSplit & virStringJoin methods
 * are derived from g_strsplit / g_strjoin in glib2,
 * also available under the LGPLv2+ license terms
 */

/**
 * virStringSplit:
 * @string: a string to split
 * @delim: a string which specifies the places at which to split
 *     the string. The delimiter is not included in any of the resulting
 *     strings, unless @max_tokens is reached.
 * @max_tokens: the maximum number of pieces to split @string into.
 *     If this is 0, the string is split completely.
 *
 * Splits a string into a maximum of @max_tokens pieces, using the given
 * @delim. If @max_tokens is reached, the remainder of @string is
 * appended to the last token.
 *
 * As a special case, the result of splitting the empty string "" is an empty
 * vector, not a vector containing a single string. The reason for this
 * special case is that being able to represent a empty vector is typically
 * more useful than consistent handling of empty elements. If you do need
 * to represent empty elements, you'll need to check for the empty string
 * before calling virStringSplit().
 *
 * Return value: a newly-allocated NULL-terminated array of strings. Use
 *    virStringFreeList() to free it.
 */
char **virStringSplit(const char *string,
                      const char *delim,
                      size_t max_tokens)
{
    char **tokens = NULL;
    size_t ntokens = 0;
    size_t maxtokens = 0;
    const char *remainder = string;
    char *tmp;
    size_t i;

    if (max_tokens == 0)
        max_tokens = INT_MAX;

    tmp = strstr(remainder, delim);
    if (tmp) {
        size_t delimlen = strlen(delim);

        while (--max_tokens && tmp) {
            size_t len = tmp - remainder;

            if (VIR_RESIZE_N(tokens, maxtokens, ntokens, 1) < 0)
                goto no_memory;

            if (!(tokens[ntokens] = strndup(remainder, len)))
                goto no_memory;
            ntokens++;
            remainder = tmp + delimlen;
            tmp = strstr(remainder, delim);
        }
    }
    if (*string) {
        if (VIR_RESIZE_N(tokens, maxtokens, ntokens, 1) < 0)
            goto no_memory;

        if (!(tokens[ntokens] = strdup(remainder)))
            goto no_memory;
        ntokens++;
    }

    if (VIR_RESIZE_N(tokens, maxtokens, ntokens, 1) < 0)
        goto no_memory;
    tokens[ntokens++] = NULL;

    return tokens;

no_memory:
    virReportOOMError();
    for (i = 0 ; i < ntokens ; i++)
        VIR_FREE(tokens[i]);
    VIR_FREE(tokens);
    return NULL;
}


/**
 * virStringJoin:
 * @strings: a NULL-terminated array of strings to join
 * @delim: a string to insert between each of the strings
 *
 * Joins a number of strings together to form one long string, with the
 * @delim inserted between each of them. The returned string
 * should be freed with VIR_FREE().
 *
 * Returns: a newly-allocated string containing all of the strings joined
 *     together, with @delim between them
 */
char *virStringJoin(const char **strings,
                    const char *delim)
{
    char *ret;
    virBuffer buf = VIR_BUFFER_INITIALIZER;
    while (*strings) {
        virBufferAdd(&buf, *strings, -1);
        if (*(strings+1))
            virBufferAdd(&buf, delim, -1);
        strings++;
    }
    if (virBufferError(&buf)) {
        virReportOOMError();
        return NULL;
    }
    ret = virBufferContentAndReset(&buf);
    if (!ret) {
        if (!(ret = strdup(""))) {
            virReportOOMError();
            return NULL;
        }
    }
    return ret;
}


/**
 * virStringFreeList:
 * @str_array: a NULL-terminated array of strings to free
 *
 * Frees a NULL-terminated array of strings, and the array itself.
 * If called on a NULL value, virStringFreeList() simply returns.
 */
void virStringFreeList(char **strings)
{
    char **tmp = strings;
    while (tmp && *tmp) {
        VIR_FREE(*tmp);
        tmp++;
    }
    VIR_FREE(strings);
}


size_t virStringListLength(char **strings)
{
    size_t i = 0;

    while (strings && strings[i])
        i++;

    return i;
}

/**
 * virStrdup:
 * @dest: where to store duplicated string
 * @src: the source string to duplicate
 * @report: whether to report OOM error, if there is one
 * @domcode: error domain code
 * @filename: caller's filename
 * @funcname: caller's funcname
 * @linenr: caller's line number
 *
 * Wrapper over strdup, which reports OOM error if told so,
 * in which case callers wants to pass @domcode, @filename,
 * @funcname and @linenr which should represent location in
 * caller's body where virStrdup is called from. Consider
 * using VIR_STRDUP which sets these automatically.
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
virStrdup(char **dest,
          const char *src,
          bool report,
          int domcode,
          const char *filename,
          const char *funcname,
          size_t linenr)
{
    if (!(*dest = strdup(src))) {
        if (report)
            virReportOOMErrorFull(domcode, filename, funcname, linenr);
        return -1;
    }

    return 0;
}

/**
 * virStrndup:
 * @dest: where to store duplicated string
 * @src: the source string to duplicate
 * @n: how many bytes to copy
 * @report: whether to report OOM error, if there is one
 * @domcode: error domain code
 * @filename: caller's filename
 * @funcname: caller's funcname
 * @linenr: caller's line number
 *
 * Wrapper over strndup, which reports OOM error if told so,
 * in which case callers wants to pass @domcode, @filename,
 * @funcname and @linenr which should represent location in
 * caller's body where virStrndup is called from. Consider
 * using VIR_STRNDUP which sets these automatically.
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
virStrndup(char **dest,
           const char *src,
           size_t n,
           bool report,
           int domcode,
           const char *filename,
           const char *funcname,
           size_t linenr)
{
    if (!(*dest = strndup(src, n))) {
        if (report)
            virReportOOMErrorFull(domcode, filename, funcname, linenr);
        return -1;
    }

   return 0;
}
