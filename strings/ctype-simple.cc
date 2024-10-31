/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "m_string.h"
#include "my_compiler.h"
#include "my_sys.h" /* Needed for MY_ERRNO_ERANGE */
#include "mysql/strings/dtoa.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/strings/my_strtoll10.h"
#include "strings/m_ctype_internals.h"
#include "template_utils.h"

MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4146 unary minus operator applied to unsigned type,
// result still unsigned
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
static inline longlong ulonglong_with_sign(bool negative, ulonglong ll) {
  return negative ? -ll : ll;
}
MY_COMPILER_DIAGNOSTIC_POP()

/*
  Returns the number of bytes required for strnxfrm().
*/

size_t my_strnxfrmlen_simple(const CHARSET_INFO *cs, size_t len) {
  return len * (cs->strxfrm_multiply ? cs->strxfrm_multiply : 1);
}

/*
  Converts a string into its sort key.

  SYNOPSIS
     my_strnxfrm_xxx()

  IMPLEMENTATION

     The my_strxfrm_xxx() function transforms a string pointed to by
     'src' with length 'srclen' according to the charset+collation
     pair 'cs' and copies the result key into 'dest'.

     Comparing two strings using memcmp() after my_strnxfrm_xxx()
     is equal to comparing two original strings with my_strnncollsp_xxx().

     Not more than 'dstlen' bytes are written into 'dst'.
     To guarantee that the whole string is transformed, 'dstlen' must be
     at least srclen*cs->strnxfrm_multiply bytes long. Otherwise,
     consequent memcmp() may return a non-accurate result.

     If the source string is too short to fill whole 'dstlen' bytes,
     then the 'dest' string is padded up to 'dstlen', ensuring that:

       "a"  == "a "
       "a\0" < "a"
       "a\0" < "a "

     my_strnxfrm_simple() is implemented for 8bit charsets and
     simple collations with one-to-one string->key transformation.

     See also implementations for various charsets/collations in
     other ctype-xxx.c files.

  RETURN

    Target len 'dstlen'.

*/

size_t my_strnxfrm_simple(const CHARSET_INFO *cs, uint8_t *dst, size_t dstlen,
                          unsigned nweights, const uint8_t *src, size_t srclen,
                          unsigned flags) {
  const uint8_t *map = cs->sort_order;
  uint8_t *d0 = dst;
  size_t frmlen = 0;
  if ((frmlen = std::min<size_t>(dstlen, nweights)) > srclen) frmlen = srclen;
  const uint8_t *end = src + frmlen;

  // Do the first few bytes.
  const uint8_t *remainder = src + (frmlen % 8);
  for (; src < remainder;) *dst++ = map[*src++];

  // Unroll loop for rest of string.
  for (; src < end;) {
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
    *dst++ = map[*src++];
  }
  return my_strxfrm_pad(cs, d0, dst, d0 + dstlen, (unsigned)(nweights - frmlen),
                        flags);
}

int my_strnncoll_simple(const CHARSET_INFO *cs, const uint8_t *s, size_t slen,
                        const uint8_t *t, size_t tlen, bool t_is_prefix) {
  size_t len = (slen > tlen) ? tlen : slen;
  const uint8_t *map = cs->sort_order;
  if (t_is_prefix && slen > tlen) slen = tlen;
  while (len--) {
    if (map[*s++] != map[*t++]) return ((int)map[s[-1]] - (int)map[t[-1]]);
  }
  /*
    We can't use (slen - tlen) here as the result may be outside of the
    precision of a signed int
  */
  return slen > tlen ? 1 : slen < tlen ? -1 : 0;
}

/*
  Compare strings, discarding end space

  SYNOPSIS
    my_strnncollsp_simple()
    cs			character set handler
    a			First string to compare
    a_length		Length of 'a'
    b			Second string to compare
    b_length		Length of 'b'

  IMPLEMENTATION
    If one string is shorter as the other, then we space extend the other
    so that the strings have equal length.

    This will ensure that the following things hold:

    "a"  == "a "
    "a\0" < "a"
    "a\0" < "a "

  RETURN
    < 0	 a <  b
    = 0	 a == b
    > 0	 a > b
*/

int my_strnncollsp_simple(const CHARSET_INFO *cs, const uint8_t *a,
                          size_t a_length, const uint8_t *b, size_t b_length) {
  const uint8_t *map = cs->sort_order;
  size_t length;
  int res;

  const uint8_t *end = a + (length = std::min(a_length, b_length));
  while (a < end) {
    if (map[*a++] != map[*b++]) return ((int)map[a[-1]] - (int)map[b[-1]]);
  }
  res = 0;
  if (a_length != b_length) {
    int swap = 1;
    /*
      Check the next not space character of the longer key. If it's < ' ',
      then it's smaller than the other key.
    */
    if (a_length < b_length) {
      /* put shorter key in s */
      a_length = b_length;
      a = b;
      swap = -1; /* swap sign of result */
      res = -res;
    }
    for (end = a + a_length - length; a < end; a++) {
      if (map[*a] != map[static_cast<int>(' ')])
        return (map[*a] < map[static_cast<int>(' ')]) ? -swap : swap;
    }
  }
  return res;
}

size_t my_caseup_str_8bit(const CHARSET_INFO *cs, char *str) {
  const uint8_t *map = cs->to_upper;
  char *str_orig = str;
  while ((*str = (char)map[(uint8_t)*str]) != 0) str++;
  return (size_t)(str - str_orig);
}

size_t my_casedn_str_8bit(const CHARSET_INFO *cs, char *str) {
  const uint8_t *map = cs->to_lower;
  char *str_orig = str;
  while ((*str = (char)map[(uint8_t)*str]) != 0) str++;
  return (size_t)(str - str_orig);
}

size_t my_caseup_8bit(const CHARSET_INFO *cs, char *src, size_t srclen,
                      char *dst [[maybe_unused]],
                      size_t dstlen [[maybe_unused]]) {
  char *end = src + srclen;
  const uint8_t *map = cs->to_upper;
  assert(src == dst && srclen == dstlen);
  for (; src != end; src++) *src = (char)map[(uint8_t)*src];
  return srclen;
}

size_t my_casedn_8bit(const CHARSET_INFO *cs, char *src, size_t srclen,
                      char *dst [[maybe_unused]],
                      size_t dstlen [[maybe_unused]]) {
  char *end = src + srclen;
  const uint8_t *map = cs->to_lower;
  assert(src == dst && srclen == dstlen);
  for (; src != end; src++) *src = (char)map[(uint8_t)*src];
  return srclen;
}

int my_strcasecmp_8bit(const CHARSET_INFO *cs, const char *s, const char *t) {
  const uint8_t *map = cs->to_upper;
  while (map[(uint8_t)*s] == map[(uint8_t)*t++])
    if (!*s++) return 0;
  return ((int)map[(uint8_t)s[0]] - (int)map[(uint8_t)t[-1]]);
}

int my_mb_wc_8bit(const CHARSET_INFO *cs, my_wc_t *wc, const uint8_t *str,
                  const uint8_t *end) {
  if (str >= end) return MY_CS_TOOSMALL;

  *wc = cs->tab_to_uni[*str];
  return (!wc[0] && str[0]) ? -1 : 1;
}

int my_wc_mb_8bit(const CHARSET_INFO *cs, my_wc_t wc, uint8_t *str,
                  uint8_t *end) {
  const MY_UNI_IDX *idx;

  if (str >= end) return MY_CS_TOOSMALL;

  for (idx = cs->tab_from_uni; idx->tab; idx++) {
    if (idx->from <= wc && idx->to >= wc) {
      str[0] = idx->tab[wc - idx->from];
      return (!str[0] && wc) ? MY_CS_ILUNI : 1;
    }
  }
  return MY_CS_ILUNI;
}

/*
   We can't use vsprintf here as it's not guaranteed to return
   the length on all operating systems.
   This function is also not called in a safe environment, so the
   end buffer must be checked.
*/

size_t my_snprintf_8bit(const CHARSET_INFO *cs [[maybe_unused]], char *to,
                        size_t n, const char *fmt, ...) {
  va_list args;
  size_t result;
  va_start(args, fmt);
  result = vsnprintf(to, n, fmt, args);
  va_end(args);
  return result;
}

void my_hash_sort_simple(const CHARSET_INFO *cs, const uint8_t *key, size_t len,
                         uint64_t *nr1, uint64_t *nr2) {
  const uint8_t *sort_order = cs->sort_order;
  /*
    Remove end space. We have to do this to be able to compare
    'A ' and 'A' as identical
  */
  const uint8_t *end = skip_trailing_space(key, len);

  uint64_t tmp1 = *nr1;
  uint64_t tmp2 = *nr2;

  for (; key < end; key++) {
    tmp1 ^= (uint64_t)((((unsigned)tmp1 & 63) + tmp2) *
                       ((unsigned)sort_order[(unsigned)*key])) +
            (tmp1 << 8);
    tmp2 += 3;
  }

  *nr1 = tmp1;
  *nr2 = tmp2;
}

long my_strntol_8bit(const CHARSET_INFO *cs, const char *nptr, size_t l,
                     int base, const char **endptr, int *err) {
  int negative = 0;
  uint32_t cutoff = 0;
  unsigned cutlim = 0;
  uint32_t i = 0;
  const char *save = nullptr;
  int overflow = 0;

  *err = 0; /* Initialize error indicator */

  const char *s = nptr;
  const char *e = nptr + l;

  for (; s < e && my_isspace(cs, *s); s++)
    ;

  if (s == e) {
    goto noconv;
  }

  /* Check for a sign.	*/
  if (*s == '-') {
    negative = 1;
    ++s;
  } else if (*s == '+') {
    negative = 0;
    ++s;
  } else
    negative = 0;

  save = s;
  cutoff = ((uint32_t)~0L) / (uint32_t)base;
  cutlim = (unsigned)(((uint32_t)~0L) % (uint32_t)base);

  overflow = 0;
  i = 0;
  for (uint8_t c = *s; s != e; c = *++s) {
    if (c >= '0' && c <= '9')
      c -= '0';
    else if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base) break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else {
      i *= (uint32_t)base;
      i += c;
    }
  }

  if (s == save) goto noconv;

  if (endptr != nullptr) *endptr = s;

  if (negative) {
    if (i > (uint32_t)std::numeric_limits<int32_t>::min()) overflow = 1;
  } else if (i > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    overflow = 1;

  if (overflow) {
    err[0] = ERANGE;
    return negative ? std::numeric_limits<int32_t>::min()
                    : std::numeric_limits<int32_t>::max();
  }

  return (negative ? -((long)i) : (long)i);

noconv:
  err[0] = EDOM;
  if (endptr != nullptr) *endptr = nptr;
  return 0L;
}

unsigned long my_strntoul_8bit(const CHARSET_INFO *cs, const char *nptr,
                               size_t l, int base, const char **endptr,
                               int *err) {
  int negative;
  uint32_t cutoff = 0;
  unsigned cutlim = 0;
  uint32_t i = 0;
  const char *save = nullptr;
  int overflow = 0;

  *err = 0; /* Initialize error indicator */

  const char *s = nptr;
  const char *e = nptr + l;

  for (; s < e && my_isspace(cs, *s); s++)
    ;

  if (s == e) {
    goto noconv;
  }

  if (*s == '-') {
    negative = 1;
    ++s;
  } else if (*s == '+') {
    negative = 0;
    ++s;
  } else
    negative = 0;

  save = s;
  cutoff = ((uint32_t)~0L) / (uint32_t)base;
  cutlim = (unsigned)(((uint32_t)~0L) % (uint32_t)base);
  overflow = 0;
  i = 0;

  for (uint8_t c = *s; s != e; c = *++s) {
    if (c >= '0' && c <= '9')
      c -= '0';
    else if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base) break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else {
      i *= (uint32_t)base;
      i += c;
    }
  }

  if (s == save) goto noconv;

  if (endptr != nullptr) *endptr = s;

  if (overflow) {
    err[0] = ERANGE;
    return (~(uint32_t)0);
  }

  return (negative ? -((long)i) : (long)i);

noconv:
  err[0] = EDOM;
  if (endptr != nullptr) *endptr = nptr;
  return 0L;
}

long long my_strntoll_8bit(const CHARSET_INFO *cs, const char *nptr, size_t l,
                           int base, const char **endptr, int *err) {
  int negative;
  unsigned long long cutoff = 0;
  unsigned cutlim = 0;
  unsigned long long i = 0;
  const char *save = nullptr;
  int overflow = 0;

  *err = 0; /* Initialize error indicator */

  const char *s = nptr;
  const char *e = nptr + l;

  for (; s < e && my_isspace(cs, *s); s++)
    ;

  if (s == e) {
    goto noconv;
  }

  if (*s == '-') {
    negative = 1;
    ++s;
  } else if (*s == '+') {
    negative = 0;
    ++s;
  } else
    negative = 0;

  save = s;

  cutoff = (~(unsigned long long)0) / (unsigned long int)base;
  cutlim = (unsigned)((~(unsigned long long)0) % (unsigned long int)base);

  overflow = 0;
  i = 0;
  for (; s != e; s++) {
    uint8_t c = *s;
    if (c >= '0' && c <= '9')
      c -= '0';
    else if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base) break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else {
      i *= (unsigned long long)base;
      i += c;
    }
  }

  if (s == save) goto noconv;

  if (endptr != nullptr) *endptr = s;

  if (negative) {
    if (i > (unsigned long long)LLONG_MIN) overflow = 1;
  } else if (i > (unsigned long long)LLONG_MAX)
    overflow = 1;

  if (overflow) {
    err[0] = ERANGE;
    return negative ? LLONG_MIN : LLONG_MAX;
  }

  return ulonglong_with_sign(negative, i);

noconv:
  err[0] = EDOM;
  if (endptr != nullptr) *endptr = nptr;
  return 0LL;
}

unsigned long long my_strntoull_8bit(const CHARSET_INFO *cs, const char *nptr,
                                     size_t l, int base, const char **endptr,
                                     int *err) {
  int negative;
  unsigned long long cutoff = 0;
  unsigned cutlim = 0;
  unsigned long long i = 0;
  const char *save = nullptr;
  int overflow = 0;

  *err = 0; /* Initialize error indicator */

  const char *s = nptr;
  const char *e = nptr + l;

  for (; s < e && my_isspace(cs, *s); s++)
    ;

  if (s == e) {
    goto noconv;
  }

  if (*s == '-') {
    negative = 1;
    ++s;
  } else if (*s == '+') {
    negative = 0;
    ++s;
  } else
    negative = 0;

  save = s;

  cutoff = (~(unsigned long long)0) / (unsigned long int)base;
  cutlim = (unsigned)((~(unsigned long long)0) % (unsigned long int)base);

  overflow = 0;
  i = 0;
  for (; s != e; s++) {
    uint8_t c = *s;

    if (c >= '0' && c <= '9')
      c -= '0';
    else if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base) break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else {
      i *= (unsigned long long)base;
      i += c;
    }
  }

  if (s == save) goto noconv;

  if (endptr != nullptr) *endptr = s;

  if (overflow) {
    err[0] = ERANGE;
    return (~(unsigned long long)0);
  }

  return ulonglong_with_sign(negative, i);

noconv:
  err[0] = EDOM;
  if (endptr != nullptr) *endptr = nptr;
  return 0L;
}

/*
  Read double from string

  SYNOPSIS:
    my_strntod_8bit()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.
    end		result pointer to end of converted string
    err		Error number if failed conversion

  NOTES:
    If length is not std::numeric_limits<int32_t>::max() or str[length] != 0
  then the given str must be writeable If length ==
  std::numeric_limits<int32_t>::max() the str must be \0 terminated.

    It's implemented this way to save a buffer allocation and a memory copy.

  RETURN
    Value of number in string
*/

double my_strntod_8bit(const CHARSET_INFO *cs [[maybe_unused]], const char *str,
                       size_t length, const char **end, int *err) {
  if (length == static_cast<size_t>(std::numeric_limits<int32_t>::max()))
    length = 65535; /* Should be big enough */
  *end = str + length;
  return my_strtod(str, end, err);
}

/*
  This is a fast version optimized for the case of radix 10 / -10

  Assume len >= 1
*/

size_t my_long10_to_str_8bit(const CHARSET_INFO *cs [[maybe_unused]], char *dst,
                             size_t len, int radix, long int val) {
  char buffer[66];
  char *p, *e;
  long int new_val;
  unsigned sign = 0;
  unsigned long int uval = (unsigned long int)val;

  e = p = &buffer[sizeof(buffer) - 1];
  *p = 0;

  if (radix < 0) {
    if (val < 0) {
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = (unsigned long int)0 - uval;
      *dst++ = '-';
      len--;
      sign = 1;
    }
  }

  new_val = (long)(uval / 10);
  *--p = '0' + (char)(uval - (unsigned long)new_val * 10);
  val = new_val;

  while (val != 0) {
    new_val = val / 10;
    *--p = '0' + (char)(val - new_val * 10);
    val = new_val;
  }

  len = std::min(len, size_t(e - p));
  memcpy(dst, p, len);
  return len + sign;
}

size_t my_longlong10_to_str_8bit(const CHARSET_INFO *cs [[maybe_unused]],
                                 char *dst, size_t len, int radix,
                                 long long val) {
  char buffer[65];
  char *p, *e;
  long long_val;
  unsigned sign = 0;
  unsigned long long uval = (unsigned long long)val;

  if (radix < 0) {
    if (val < 0) {
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = (unsigned long long)0 - uval;
      *dst++ = '-';
      len--;
      sign = 1;
    }
  }

  e = p = &buffer[sizeof(buffer) - 1];
  *p = 0;

  if (uval == 0) {
    *--p = '0';
    len = 1;
    goto cnv;
  }

  while (uval > (unsigned long long)LONG_MAX) {
    unsigned long long quo = uval / (unsigned)10;
    unsigned rem = (unsigned)(uval - quo * (unsigned)10);
    *--p = '0' + rem;
    uval = quo;
  }

  long_val = (long)uval;
  while (long_val != 0) {
    long quo = long_val / 10;
    *--p = (char)('0' + (long_val - quo * 10));
    long_val = quo;
  }

  len = std::min(len, size_t(e - p));
cnv:
  memcpy(dst, p, len);
  return len + sign;
}

/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#define likeconv(s, A) (uint8_t)(s)->sort_order[(uint8_t)(A)]
#define INC_PTR(cs, A, B) (A)++

static int my_wildcmp_8bit_impl(const CHARSET_INFO *cs, const char *str,
                                const char *str_end, const char *wildstr_arg,
                                const char *wildend_arg, int escape, int w_one,
                                int w_many, int recurse_level) {
  int result = -1; /* Not found, using wildcards */
  const uint8_t *wildstr = pointer_cast<const uint8_t *>(wildstr_arg);
  const uint8_t *wildend = pointer_cast<const uint8_t *>(wildend_arg);

  if (my_string_stack_guard && my_string_stack_guard(recurse_level)) return -1;
  while (wildstr != wildend) {
    while (*wildstr != w_many && *wildstr != w_one) {
      if (*wildstr == escape && wildstr + 1 != wildend) wildstr++;

      if (str == str_end || likeconv(cs, *wildstr++) != likeconv(cs, *str++))
        return (1); /* No match */
      if (wildstr == wildend)
        return (str != str_end); /* Match if both are at end */
      result = 1;                /* Found an anchor char     */
    }
    if (*wildstr == w_one) {
      do {
        if (str == str_end) /* Skip one char if possible */
          return (result);
        INC_PTR(cs, str, str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend) break;
    }
    if (*wildstr == w_many) { /* Found w_many */
      uint8_t cmp = 0;

      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend; wildstr++) {
        if (*wildstr == w_many) continue;
        if (*wildstr == w_one) {
          if (str == str_end) return (-1);
          INC_PTR(cs, str, str_end);
          continue;
        }
        break; /* Not a wild character */
      }
      if (wildstr == wildend) return (0); /* Ok if w_many is last */
      if (str == str_end) return (-1);

      if ((cmp = *wildstr) == escape && wildstr + 1 != wildend)
        cmp = *++wildstr;

      INC_PTR(cs, wildstr, wildend); /* This is compared through cmp */
      cmp = likeconv(cs, cmp);
      do {
        while (str != str_end && (uint8_t)likeconv(cs, *str) != cmp) str++;
        if (str++ == str_end) return (-1);
        {
          int tmp = my_wildcmp_8bit_impl(
              cs, str, str_end, pointer_cast<const char *>(wildstr),
              wildend_arg, escape, w_one, w_many, recurse_level + 1);
          if (tmp <= 0) return (tmp);
        }
      } while (str != str_end);
      return (-1);
    }
  }
  return (str != str_end ? 1 : 0);
}

int my_wildcmp_8bit(const CHARSET_INFO *cs, const char *str,
                    const char *str_end, const char *wildstr,
                    const char *wildend, int escape, int w_one, int w_many) {
  return my_wildcmp_8bit_impl(cs, str, str_end, wildstr, wildend, escape, w_one,
                              w_many, 1);
}

/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from min_str and max_str
** res_length	Length of min_str and max_str.
** min_str	Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

bool my_like_range_simple(const CHARSET_INFO *cs, const char *ptr,
                          size_t ptr_length, char escape, char w_one,
                          char w_many, size_t res_length, char *min_str,
                          char *max_str, size_t *min_length,
                          size_t *max_length) {
  const char *end = ptr + ptr_length;
  char *min_org = min_str;
  char *min_end = min_str + res_length;
  size_t charlen = res_length / cs->mbmaxlen;

  for (; ptr != end && min_str != min_end && charlen > 0; ptr++, charlen--) {
    if (*ptr == escape && ptr + 1 != end) {
      ptr++; /* Skip escape */
      *min_str++ = *max_str++ = *ptr;
      continue;
    }
    if (*ptr == w_one) /* '_' in SQL */
    {
      *min_str++ = '\0'; /* This should be min char */
      *max_str++ = (char)cs->max_sort_char;
      continue;
    }
    if (*ptr == w_many) /* '%' in SQL */
    {
      /* Calculate length of keys */
      *min_length = ((cs->state & MY_CS_BINSORT) ? (size_t)(min_str - min_org)
                                                 : res_length);
      *max_length = res_length;
      do {
        *min_str++ = 0;
        *max_str++ = (char)cs->max_sort_char;
      } while (min_str != min_end);
      return false;
    }
    *min_str++ = *max_str++ = *ptr;
  }

  *min_length = *max_length = (size_t)(min_str - min_org);
  while (min_str != min_end)
    *min_str++ = *max_str++ = ' '; /* Because if key compression */
  return false;
}

size_t my_scan_8bit(const CHARSET_INFO *cs, const char *str, const char *end,
                    int sq) {
  const char *str0 = str;
  switch (sq) {
    case MY_SEQ_INTTAIL:
      if (*str == '.') {
        for (str++; str != end && *str == '0'; str++)
          ;
        return (size_t)(str - str0);
      }
      return 0;

    case MY_SEQ_SPACES:
      for (; str < end; str++) {
        if (!my_isspace(cs, *str)) break;
      }
      return (size_t)(str - str0);
    default:
      return 0;
  }
}

void my_fill_8bit(const CHARSET_INFO *cs [[maybe_unused]], char *s, size_t l,
                  int fill) {
  memset(s, fill, l);
}

size_t my_numchars_8bit(const CHARSET_INFO *cs [[maybe_unused]], const char *b,
                        const char *e) {
  return (size_t)(e - b);
}

size_t my_numcells_8bit(const CHARSET_INFO *cs [[maybe_unused]], const char *b,
                        const char *e) {
  return (size_t)(e - b);
}

size_t my_charpos_8bit(const CHARSET_INFO *cs [[maybe_unused]],
                       const char *b [[maybe_unused]],
                       const char *e [[maybe_unused]], size_t pos) {
  return pos;
}

size_t my_well_formed_len_8bit(const CHARSET_INFO *cs [[maybe_unused]],
                               const char *start, const char *end,
                               size_t nchars, int *error) {
  size_t nbytes = (size_t)(end - start);
  *error = 0;
  return std::min(nbytes, nchars);
}

size_t my_lengthsp_8bit(const CHARSET_INFO *cs [[maybe_unused]],
                        const char *ptr, size_t length) {
  const char *end = pointer_cast<const char *>(
      skip_trailing_space(pointer_cast<const uint8_t *>(ptr), length));
  return (size_t)(end - ptr);
}

bool my_instr_simple(const CHARSET_INFO *cs, const char *b, size_t b_length,
                     const char *s, size_t s_length, my_match_t *match) {
  if (s_length <= b_length) {
    if (s_length == 0) {
      if (match != nullptr) {
        match->end = 0;
        match->mb_len = 0;
      }
      return true; /* Empty string is always found */
    }

    const uint8_t *str = pointer_cast<const uint8_t *>(b);
    const uint8_t *search = pointer_cast<const uint8_t *>(s);
    const uint8_t *end =
        pointer_cast<const uint8_t *>(b) + b_length - s_length + 1;
    const uint8_t *search_end = pointer_cast<const uint8_t *>(s) + s_length;

  skip:
    while (str != end) {
      if (cs->sort_order[*str++] == cs->sort_order[*search]) {
        const uint8_t *i = str;
        const uint8_t *j = search + 1;

        while (j != search_end)
          if (cs->sort_order[*i++] != cs->sort_order[*j++]) goto skip;

        if (match != nullptr) {
          match->end = (str - pointer_cast<const uint8_t *>(b) - 1);
          match->mb_len = match->end;
        }
        return true;
      }
    }
  }
  return false;
}

extern "C" {
static size_t my_well_formed_len_ascii(const CHARSET_INFO *cs [[maybe_unused]],
                                       const char *start, const char *end,
                                       size_t nchars [[maybe_unused]],
                                       int *error) {
  const char *oldstart = start;
  *error = 0;
  while (start < end) {
    if ((*start & 0x80) != 0) {
      *error = 1;
      break;
    }
    start++;
  }
  return start - oldstart;
}
}  // extern "C"

typedef struct {
  int nchars;
  MY_UNI_IDX uidx;
} uni_idx;

#define PLANE_SIZE 0x100
#define PLANE_NUM 0x100
#define PLANE_NUMBER(x) (((x) >> 8) % PLANE_NUM)

static int pcmp(const void *f, const void *s) {
  const uni_idx *F = (const uni_idx *)f;
  const uni_idx *S = (const uni_idx *)s;
  int res;

  if (!(res = ((S->nchars) - (F->nchars))))
    res = ((F->uidx.from) - (S->uidx.to));
  return res;
}

static bool create_fromuni(CHARSET_INFO *cs, MY_CHARSET_LOADER *loader) {
  uni_idx idx[PLANE_NUM];
  int i, n;
  MY_UNI_IDX *tab_from_uni;

  /*
    Check that Unicode map is loaded.
    It can be not loaded when the collation is
    listed in Index.xml but not specified
    in the character set specific XML file.
  */
  if (!cs->tab_to_uni) return true;

  /* Clear plane statistics */
  memset(idx, 0, sizeof(idx));

  /* Count number of characters in each plane */
  for (i = 0; i < 0x100; i++) {
    uint16_t wc = cs->tab_to_uni[i];
    int pl = PLANE_NUMBER(wc);

    if (wc || !i) {
      if (!idx[pl].nchars) {
        idx[pl].uidx.from = wc;
        idx[pl].uidx.to = wc;
      } else {
        idx[pl].uidx.from = wc < idx[pl].uidx.from ? wc : idx[pl].uidx.from;
        idx[pl].uidx.to = wc > idx[pl].uidx.to ? wc : idx[pl].uidx.to;
      }
      idx[pl].nchars++;
    }
  }

  /* Sort planes in descending order */
  qsort(&idx, PLANE_NUM, sizeof(uni_idx), &pcmp);

  for (i = 0; i < PLANE_NUM; i++) {
    int ch, numchars;
    uint8_t *tab = nullptr;

    /* Skip empty plane */
    if (!idx[i].nchars) break;

    numchars = idx[i].uidx.to - idx[i].uidx.from + 1;
    if (!(idx[i].uidx.tab = tab = static_cast<uint8_t *>(
              (loader->once_alloc)(numchars * sizeof(*idx[i].uidx.tab)))))
      return true;

    memset(tab, 0, numchars * sizeof(*idx[i].uidx.tab));

    for (ch = 1; ch < PLANE_SIZE; ch++) {
      uint16_t wc = cs->tab_to_uni[ch];
      if (wc >= idx[i].uidx.from && wc <= idx[i].uidx.to && wc) {
        int ofs = wc - idx[i].uidx.from;
        /*
          Character sets like armscii8 may have two code points for
          one character. When converting from UNICODE back to
          armscii8, select the lowest one, which is in the ASCII
          range.
        */
        if (tab[ofs] == '\0') tab[ofs] = ch;
      }
    }
  }

  /* Allocate and fill reverse table for each plane */
  n = i;
  if (!(cs->tab_from_uni = tab_from_uni =
            (MY_UNI_IDX *)loader->once_alloc(sizeof(MY_UNI_IDX) * (n + 1))))
    return true;

  for (i = 0; i < n; i++) tab_from_uni[i] = idx[i].uidx;

  /* Set end-of-list marker */
  memset(&tab_from_uni[i], 0, sizeof(MY_UNI_IDX));
  return false;
}

extern "C" {
static bool my_cset_init_8bit(CHARSET_INFO *cs, MY_CHARSET_LOADER *loader,
                              MY_CHARSET_ERRMSG *) {
  cs->caseup_multiply = 1;
  cs->casedn_multiply = 1;
  cs->pad_char = ' ';
  return create_fromuni(cs, loader);
}
}  // extern "C"

static void set_max_sort_char(CHARSET_INFO *cs) {
  if (!cs->sort_order) return;

  uint8_t max_char = cs->sort_order[(uint8_t)cs->max_sort_char];
  for (unsigned i = 0; i < 256; i++) {
    if ((uint8_t)cs->sort_order[i] > max_char) {
      max_char = (uint8_t)cs->sort_order[i];
      cs->max_sort_char = i;
    }
  }
}

extern "C" {
static bool my_coll_init_simple(CHARSET_INFO *cs, MY_CHARSET_LOADER *,
                                MY_CHARSET_ERRMSG *) {
  set_max_sort_char(cs);
  return false;
}
}  // extern "C"

long long my_strtoll10_8bit(const CHARSET_INFO *cs [[maybe_unused]],
                            const char *nptr, const char **endptr, int *error) {
  return my_strtoll10(nptr, endptr, error);
}

int my_mb_ctype_8bit(const CHARSET_INFO *cs, int *ctype, const uint8_t *s,
                     const uint8_t *e) {
  if (s >= e) {
    *ctype = 0;
    return MY_CS_TOOSMALL;
  }
  *ctype = cs->ctype[*s + 1];
  return 1;
}

constexpr const uint64_t CUTOFF{ULLONG_MAX / 10};
constexpr const uint64_t CUTLIM{ULLONG_MAX % 10};
constexpr const int DIGITS_IN_ULONGLONG{20};

static unsigned long long d10[DIGITS_IN_ULONGLONG] = {1,
                                                      10,
                                                      100,
                                                      1000,
                                                      10000,
                                                      100000,
                                                      1000000,
                                                      10000000,
                                                      100000000,
                                                      1000000000,
                                                      10000000000ULL,
                                                      100000000000ULL,
                                                      1000000000000ULL,
                                                      10000000000000ULL,
                                                      100000000000000ULL,
                                                      1000000000000000ULL,
                                                      10000000000000000ULL,
                                                      100000000000000000ULL,
                                                      1000000000000000000ULL,
                                                      10000000000000000000ULL};

/*

  Convert a string to unsigned long long integer value
  with rounding.

  SYNOPSIS
    my_strntoull10_8bit()
      cs              in      pointer to character set
      str             in      pointer to the string to be converted
      length          in      string length
      unsigned_flag   in      whether the number is unsigned
      endptr          out     pointer to the stop character
      error           out     returned error code

  DESCRIPTION
    This function takes the decimal representation of integer number
    from string str and converts it to an signed or unsigned
    long long integer value.
    Space characters and tab are ignored.
    A sign character might precede the digit characters.
    The number may have any number of pre-zero digits.
    The number may have decimal point and exponent.
    Rounding is always done in "away from zero" style:
      0.5  ->   1
     -0.5  ->  -1

    The function stops reading the string str after "length" bytes
    or at the first character that is not a part of correct number syntax:

    <signed numeric literal> ::=
      [ <sign> ] <exact numeric literal> [ E [ <sign> ] <unsigned integer> ]

    <exact numeric literal> ::=
                        <unsigned integer> [ <period> [ <unsigned integer> ] ]
                      | <period> <unsigned integer>
    <unsigned integer>   ::= <digit>...

  RETURN VALUES
    Value of string as a signed/unsigned long long integer

    endptr cannot be NULL. The function will store the end pointer
    to the stop character here.

    The error parameter contains information how things went:
    0	     ok
    ERANGE   If the the value of the converted number is out of range
    In this case the return value is:
    - ULLONG_MAX if unsigned_flag and the number was too big
    - 0 if unsigned_flag and the number was negative
    - LLONG_MAX if no unsigned_flag and the number is too big
    - LLONG_MIN if no unsigned_flag and the number it too big negative

    EDOM If the string didn't contain any digits.
    In this case the return value is 0.
*/

unsigned long long my_strntoull10rnd_8bit(const CHARSET_INFO *cs
                                          [[maybe_unused]],
                                          const char *str, size_t length,
                                          int unsigned_flag,
                                          const char **endptr, int *error) {
  const char *dot, *end9, *beg, *end = str + length;
  unsigned long long ull = 0;
  unsigned long ul = 0;
  uint8_t ch = 0;
  int shift = 0;
  int digits = 0;
  int addon = 0;
  bool negative = false;

  /* Skip leading spaces and tabs */
  for (; str < end && (*str == ' ' || *str == '\t'); str++)
    ;

  if (str >= end) goto ret_edom;

  if ((negative = (*str == '-')) || *str == '+') /* optional sign */
  {
    if (++str == end) goto ret_edom;
  }

  beg = str;
  end9 = (str + 9) > end ? end : (str + 9);
  /* Accumulate small number into unsigned long, for performance purposes */
  for (ul = 0; str < end9 && (ch = (uint8_t)(*str - '0')) < 10; str++) {
    ul = ul * 10 + ch;
  }

  if (str >= end) /* Small number without dots and expanents */
  {
    *endptr = str;
    if (negative) {
      if (unsigned_flag) {
        *error = ul ? MY_ERRNO_ERANGE : 0;
        return 0;
      } else {
        *error = 0;
        return (unsigned long long)(long long)-(long)ul;
      }
    } else {
      *error = 0;
      return (unsigned long long)ul;
    }
  }

  digits = (int)(str - beg);

  /* Continue to accumulate into unsigned long long */
  for (dot = nullptr, ull = ul; str < end; str++) {
    if ((ch = (uint8_t)(*str - '0')) < 10) {
      if (ull < CUTOFF || (ull == CUTOFF && ch <= CUTLIM)) {
        ull = ull * 10 + ch;
        digits++;
        continue;
      }
      /*
        Adding the next digit would overflow.
        Remember the next digit in "addon", for rounding.
        Scan all digits with an optional single dot.
      */
      if (ull == CUTOFF) {
        ull = ULLONG_MAX;
        addon = 1;
        str++;
      } else
        addon = (*str >= '5');
      if (!dot) {
        for (; str < end && ((uint8_t)(*str - '0')) < 10; shift++, str++)
          ;
        if (str < end && *str == '.') {
          str++;
          for (; str < end && ((uint8_t)(*str - '0')) < 10; str++)
            ;
        }
      } else {
        shift = (int)(dot - str);
        for (; str < end && ((uint8_t)(*str - '0')) < 10; str++)
          ;
      }
      goto exp;
    }

    if (*str == '.') {
      if (dot) {
        /* The second dot character */
        goto dotshift;
      } else {
        dot = str + 1;
      }
      continue;
    }

    /* Unknown character, exit the loop */
    break;
  }

dotshift:
  shift = dot ? (int)(dot - str) : 0; /* Right shift */
  addon = 0;

exp: /* [ E [ <sign> ] <unsigned integer> ] */

  if (!digits) {
    str = beg;
    goto ret_edom;
  }

  if (str < end && (*str == 'e' || *str == 'E')) {
    str++;
    if (str < end) {
      long long negative_exp = 0;
      long long exponent = 0;
      if ((negative_exp = (*str == '-')) || *str == '+') {
        if (++str == end) goto check_shift_overflow;
      }
      for (exponent = 0; str < end && (ch = (uint8_t)(*str - '0')) < 10;
           str++) {
        if (exponent <= (std::numeric_limits<long long>::max() - ch) / 10)
          exponent = exponent * 10 + ch;
        else
          goto ret_too_big;
      }
      shift += negative_exp ? -exponent : exponent;
    }
  }

  if (shift == 0) /* No shift, check addon digit */
  {
    if (addon) {
      if (ull == ULLONG_MAX) goto ret_too_big;
      ull++;
    }
    goto ret_sign;
  }

  if (shift < 0) /* Right shift */
  {
    if (shift == std::numeric_limits<int32_t>::min() ||
        -shift >= DIGITS_IN_ULONGLONG)
      goto ret_zero; /* Exponent is a big negative number, return 0 */

    uint64_t d = d10[-shift];
    uint64_t r = ull % d;
    ull /= d;
    if (r >= d / 2) ull++;
    goto ret_sign;
  }

check_shift_overflow:
  if (shift > DIGITS_IN_ULONGLONG) /* Huge left shift */
  {
    if (!ull) goto ret_sign;
    goto ret_too_big;
  }

  for (; shift > 0; shift--, ull *= 10) /* Left shift */
  {
    if (ull > CUTOFF) goto ret_too_big; /* Overflow, number too big */
  }

ret_sign:
  *endptr = str;

  if (!unsigned_flag) {
    if (negative) {
      if (ull > (unsigned long long)LLONG_MIN) {
        *error = MY_ERRNO_ERANGE;
        return (unsigned long long)LLONG_MIN;
      }
      *error = 0;
      if (ull == static_cast<unsigned long long>(LLONG_MIN))
        return static_cast<unsigned long long>(LLONG_MIN);
      return (unsigned long long)-(long long)ull;
    } else {
      if (ull > (unsigned long long)LLONG_MAX) {
        *error = MY_ERRNO_ERANGE;
        return (unsigned long long)LLONG_MAX;
      }
      *error = 0;
      return ull;
    }
  }

  /* Unsigned number */
  if (negative && ull) {
    *error = MY_ERRNO_ERANGE;
    return 0;
  }
  *error = 0;
  return ull;

ret_zero:
  *endptr = str;
  *error = 0;
  return 0;

ret_edom:
  *endptr = str;
  *error = MY_ERRNO_EDOM;
  return 0;

ret_too_big:
  *endptr = str;
  *error = MY_ERRNO_ERANGE;
  if (unsigned_flag) {
    if (negative) return 0;
    return ULLONG_MAX;
  } else {
    if (negative) return LLONG_MIN;
    return LLONG_MAX;
  }
}

/*
  Check if a constant can be propagated

  SYNOPSIS:
    my_propagate_simple()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.

  NOTES:
   Takes the string in the given charset and check
   if it can be safely propagated in the optimizer.

   create table t1 (
     s char(5) character set latin1 collate latin1_german2_ci);
   insert into t1 values (0xf6); -- o-umlaut
   select * from t1 where length(s)=1 and s='oe';

   The above query should return one row.
   We cannot convert this query into:
   select * from t1 where length('oe')=1 and s='oe';

   Currently we don't check the constant itself,
   and decide not to propagate a constant
   just if the collation itself allows tricky things
   like expansions and contractions. In the future
   we can write a more sophisticated functions to
   check the constants. For example, 'oa' can always
   be safety propagated in German2 because unlike
   'oe' it does not have any special meaning.

  RETURN
    1 if constant can be safely propagated
    0 if it is not safe to propagate the constant
*/

bool my_propagate_simple(const CHARSET_INFO *cs [[maybe_unused]],
                         const uint8_t *str [[maybe_unused]],
                         size_t length [[maybe_unused]]) {
  return true;
}

bool my_propagate_complex(const CHARSET_INFO *cs [[maybe_unused]],
                          const uint8_t *str [[maybe_unused]],
                          size_t length [[maybe_unused]]) {
  return false;
}

/*
  Normalize strxfrm flags

  SYNOPSIS:
    my_strxfrm_flag_normalize()
    flags    - non-normalized flags

  RETURN
    normalized flags
*/

unsigned my_strxfrm_flag_normalize(unsigned flags) {
  flags &= MY_STRXFRM_PAD_TO_MAXLEN;
  return flags;
}

size_t my_strxfrm_pad(const CHARSET_INFO *cs, uint8_t *str, uint8_t *frmend,
                      uint8_t *strend, unsigned nweights, unsigned flags) {
  if (nweights && frmend < strend) {
    // PAD SPACE behavior.
    unsigned fill_length =
        std::min<unsigned>(strend - frmend, nweights * cs->mbminlen);
    cs->cset->fill(cs, (char *)frmend, fill_length, cs->pad_char);
    frmend += fill_length;
  }
  if ((flags & MY_STRXFRM_PAD_TO_MAXLEN) && frmend < strend) {
    size_t fill_length = strend - frmend;
    cs->cset->fill(cs, (char *)frmend, fill_length, cs->pad_char);
    frmend = strend;
  }
  return frmend - str;
}

MY_CHARSET_HANDLER my_charset_8bit_handler = {my_cset_init_8bit,
                                              nullptr, /* ismbchar      */
                                              my_mbcharlen_8bit, /* mbcharlen */
                                              my_numchars_8bit,
                                              my_charpos_8bit,
                                              my_well_formed_len_8bit,
                                              my_lengthsp_8bit,
                                              my_numcells_8bit,
                                              my_mb_wc_8bit,
                                              my_wc_mb_8bit,
                                              my_mb_ctype_8bit,
                                              my_caseup_str_8bit,
                                              my_casedn_str_8bit,
                                              my_caseup_8bit,
                                              my_casedn_8bit,
                                              my_snprintf_8bit,
                                              my_long10_to_str_8bit,
                                              my_longlong10_to_str_8bit,
                                              my_fill_8bit,
                                              my_strntol_8bit,
                                              my_strntoul_8bit,
                                              my_strntoll_8bit,
                                              my_strntoull_8bit,
                                              my_strntod_8bit,
                                              my_strtoll10_8bit,
                                              my_strntoull10rnd_8bit,
                                              my_scan_8bit};

MY_CHARSET_HANDLER my_charset_ascii_handler = {
    my_cset_init_8bit,
    nullptr,           /* ismbchar      */
    my_mbcharlen_8bit, /* mbcharlen     */
    my_numchars_8bit,
    my_charpos_8bit,
    my_well_formed_len_ascii,
    my_lengthsp_8bit,
    my_numcells_8bit,
    my_mb_wc_8bit,
    my_wc_mb_8bit,
    my_mb_ctype_8bit,
    my_caseup_str_8bit,
    my_casedn_str_8bit,
    my_caseup_8bit,
    my_casedn_8bit,
    my_snprintf_8bit,
    my_long10_to_str_8bit,
    my_longlong10_to_str_8bit,
    my_fill_8bit,
    my_strntol_8bit,
    my_strntoul_8bit,
    my_strntoll_8bit,
    my_strntoull_8bit,
    my_strntod_8bit,
    my_strtoll10_8bit,
    my_strntoull10rnd_8bit,
    my_scan_8bit};

MY_COLLATION_HANDLER my_collation_8bit_simple_ci_handler = {
    my_coll_init_simple, /* init */
    nullptr,
    my_strnncoll_simple,
    my_strnncollsp_simple,
    my_strnxfrm_simple,
    my_strnxfrmlen_simple,
    my_like_range_simple,
    my_wildcmp_8bit,
    my_strcasecmp_8bit,
    my_instr_simple,
    my_hash_sort_simple,
    my_propagate_simple};
