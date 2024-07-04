/*
 * Copyright (c) 2000-2012 Marc Alexander Lehmann <schmorp@schmorp.de>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

#include <stdint.h>

#define LZF_MAX_LIT (1 <<  5)
#define LZF_MAX_OFF (1 << 13)
#define LZF_MAX_REF ((1 << 8) + (1 << 3))

#define HASH(p) (p[0] << 6) ^ (p[1] << 3) ^ p[2]

/*
 * compressed format
 *
 * 000LLLLL <L+1>             ; literal L+1=1..33  bytes
 * LLLooooo oooooooo          ; backref L+1=1..7   bytes, o+1=1..8192 offset
 * 111ooooo LLLLLLLL oooooooo ; backref L+8=8..263 bytes, o+1=1..8192 offset
 *
 */

unsigned int lzf_compress(const void *const in_data, unsigned int in_len,
                   void *out_data, unsigned int out_len) {
  const uint8_t *ip = (const uint8_t *)in_data;
        uint8_t *op = (uint8_t *)out_data;
  const uint8_t *in_end  = ip + in_len;
        uint8_t *out_end = op + out_len;

  const uint8_t *first [1 << (6+8)]; /* most recent occurance of a match */
  uint16_t prev [LZF_MAX_OFF]; /* how many bytes to go backwards for the next match */

  int lit;
  if (!in_len || !out_len) return 0;
  lit = 0; op++; /* start run */
  lit++; *op++ = *ip++;
  while (ip < in_end - 2) {
    int best_l = 0;
    const uint8_t *best_p;
    int maxlen = (in_end - ip < LZF_MAX_REF ? in_end - ip : LZF_MAX_REF);
    unsigned int res = ((uintptr_t)ip) & (LZF_MAX_OFF - 1);
    uint16_t hash = HASH (ip);
    uint16_t diff;
    const uint8_t *b = ip < (uint8_t *)in_data + LZF_MAX_OFF ? in_data : ip - LZF_MAX_OFF;
    const uint8_t *p = first[hash];
    prev[res] = ip - p; /* update ptr to previous hash match */
    first[hash] = ip; /* first hash match is here */
    if (p < ip) while (p >= b) {
      if (p[2] == ip[2]) { /* first two bytes almost always match */
        if (p[best_l] == ip[best_l]) { /* new match must be longer than the old one to qualify */
          if (p[1] == ip[1] && p[0] == ip[0]) { /* just to be sure */
            int len = 3;
            while (p[len] == ip[len] && len < maxlen) ++len;
            if (len > best_l) {
                best_p = p;
                best_l = len;
                if (len >= (LZF_MAX_REF)) break; /* abort search if max match len found */
            }
          }
        }
      }
      diff = prev[((uintptr_t)p) & (LZF_MAX_OFF - 1)];
      p = diff ? p - diff : 0;
    }
    if (best_l) {
      int len = best_l;
      int off = ip - best_p - 1;
      if (op + 3 + 1 >= out_end) /* first a faster conservative test */
        if (op - !lit + 3 + 1 >= out_end) /* second the exact but rare test */
          return 0;
      op [- lit - 1] = lit - 1; /* stop run */
      op -= !lit; /* undo run if length is zero */
      len -= 2; /* len is now #octets - 1 */
      ip++;
      if (len < 7) {
        *op++ = (off >> 8) + (len << 5);
      } else {
        *op++ = (off >> 8) + (  7 << 5);
        *op++ = len - 7;
      }
      *op++ = off;
      lit = 0; op++; /* start run */
      ip += len + 1;
      if (ip >= in_end - 2) break;
      ip -= len + 1;
      do {
        uint16_t hash = HASH (ip);
        res = ((uintptr_t)ip) & (LZF_MAX_OFF - 1);
        p = first[hash];
        prev[res] = ip - p; /* update ptr to previous hash match */
        first[hash] = ip; /* first hash match is here */
        ip++;
      } while (len--);
    } else {
      /* one more literal byte we must copy */
      if (op >= out_end) return 0;
      lit++; *op++ = *ip++;
      if (lit == LZF_MAX_LIT) {
        op [- lit - 1] = lit - 1; /* stop run */
        lit = 0; op++; /* start run */
      }
    }
  }
  if (op + 3 > out_end) /* at most 3 bytes can be missing here */
    return 0;
  while (ip < in_end) {
    lit++; *op++ = *ip++;
    if (lit == LZF_MAX_LIT) {
      op [- lit - 1] = lit - 1; /* stop run */
      lit = 0; op++; /* start run */
    }
  }
  op [- lit - 1] = lit - 1; /* end run */
  op -= !lit; /* undo run if length is zero */
  return op - (uint8_t *)out_data;
}

unsigned int
lzf_decompress (const void *const in_data,  unsigned int in_len,
                void             *out_data, unsigned int out_len) {
  uint8_t const *ip = (const uint8_t *)in_data;
  uint8_t       *op = (uint8_t *)out_data;
  uint8_t const *const in_end  = ip + in_len;
  uint8_t       *const out_end = op + out_len;
  do {
    unsigned int ctrl = *ip++;
    if (ctrl < (1 << 5)) { /* literal run */
      ctrl++;
      if (op + ctrl > out_end) return 0; // depacked data too big for out_len
      if (ip + ctrl > in_end) return 0; // input error
      while (ctrl--) *op++ = *ip++;
    } else { /* back reference */
      unsigned int len = ctrl >> 5;
      uint8_t *ref = op - ((ctrl & 0x1f) << 8) - 1;
      if (ip >= in_end) return 0; // input error
      if (len == 7) {
	len += *ip++;
        if (ip >= in_end) return 0; // input error
      };
      ref -= *ip++;
      if (op + len + 2 > out_end) return 0; // depacked data too big for out_len
      if (ref < (uint8_t *)out_data) return 0; // invalid backwards reference
      len += 2;
      do *op++ = *ref++; while (--len);
    }
  } while (ip < in_end);
  return op - (uint8_t *)out_data;
}

