/*-----------------------------------------------------------------------------
 * search_trie.h
 * 
 * ----------------------------------------------------------------------------
 */

#ifndef SEARCH_TRIE__
#define SEARCH_TRIE__

#define NOT_FOUND ((unsigned int) (-1))

unsigned int search_trie (const char *trie_stream, const char *keyword);

#endif
