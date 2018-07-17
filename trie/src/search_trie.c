/*-----------------------------------------------------------------------------
 * search_trie.c
 * 
 * ----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "search_trie.h"

//#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define DBG(x) printf x;
#else
#define DBG(x)
#endif

#define XDBG(x)

unsigned int search_trie (const char *trie_stream, const char *keyword)
{
  unsigned char *ustream = (unsigned char *) trie_stream;
  unsigned char *uword = (unsigned char *) keyword;
  unsigned int keyword_len = strlen(keyword);
  unsigned int word_part_len;
  unsigned int matched_so_far = 0;
  unsigned int index_len;
  unsigned int stream_pos = 0;

  char c, d;
  
  if (0 == keyword_len) {
    DBG (("0 length keyword\n"));
    return NOT_FOUND;
  }    
  while (true) {
    int matched;
    unsigned int i;
    unsigned int rem_keyword_len;
    unsigned int word_part_pos;
    /* Find matching letter */
    index_len = ustream[stream_pos++];

    for (i=0; i<index_len; i++, stream_pos+=6) {
      if ((c=uword[matched_so_far]) < (d=ustream[stream_pos])) {
        DBG (("Key letter mismatch %c %c pos %u %u\n", 
          c, d, matched_so_far, stream_pos));
        return NOT_FOUND;
      }
      if (c == d)
        break;
    }
    if (i >= index_len) {
      DBG (("Key letter %c mismatch (2) pos %u\n", c, matched_so_far));
      return NOT_FOUND;
    }
    DBG (("Matched letter %c\n", c));
    ++matched_so_far;
    rem_keyword_len = keyword_len - matched_so_far;
    word_part_len = ((ustream[stream_pos+4] & 0x7F) << 8) + ustream[stream_pos+5];
    DBG (("rem_keywd len %u, word_part_len %u\n", rem_keyword_len, word_part_len));
    if (rem_keyword_len < word_part_len) {
      DBG (("Keyword too short\n"));
      return NOT_FOUND;
    }
    if (rem_keyword_len == word_part_len) {
      if (0 == (ustream[stream_pos+1] & 0x80)) { // if no value
        DBG (("No value found\n"));
        return NOT_FOUND;
      }
      word_part_pos = 
	((ustream[stream_pos+1] & 0x7F) << 16) +
	 (ustream[stream_pos+2] << 8) +
	 (ustream[stream_pos+3]);
      if (word_part_len == 0)
        matched = 0;
      else {
	DBG (("Word part stream pos is %u\n", word_part_pos));
        matched = strncmp (keyword+matched_so_far,
          trie_stream + word_part_pos, word_part_len);
      }
      if (0 == matched) {
	unsigned int value_pos = word_part_pos + word_part_len;
	return (ustream[value_pos] << 8) + ustream[value_pos+1];
      } else {
        DBG (("Tail of word_part mismatch (1) \n"));
	return NOT_FOUND;
      }
    } else { // rem_keyword_len > word_part_len)
      if (0 == (ustream[stream_pos+4] & 0x80)) // if no down level
        return NOT_FOUND;
      word_part_pos = 
	((ustream[stream_pos+1] & 0x7F) << 16) +
	 (ustream[stream_pos+2] << 8) +
	 (ustream[stream_pos+3]);
      DBG (("Word part stream pos is %u\n", word_part_pos));
      if (word_part_len == 0)
        matched = 0;
      else {
        matched = strncmp (keyword+matched_so_far,
          trie_stream + word_part_pos, word_part_len);
      }
      if (0 != matched) {
        DBG (("Tail of word_part mismatch (2) \n"));
	return NOT_FOUND;
      }
      matched_so_far += word_part_len;
      if (0 == (ustream[stream_pos+1] & 0x80)) // if no value
        stream_pos = word_part_pos + word_part_len;
      else
        stream_pos = word_part_pos + word_part_len + 2; // 2 more for the value length              
      DBG (("New stream pos %u\n", stream_pos));
    }
  } // end while
}
