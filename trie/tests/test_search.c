/*-----------------------------------------------------------------------------
 * test_search.c
 * 
 * ----------------------------------------------------------------------------
 */

#include <stdio.h>
#include "search_trie.h"
#include "trie3.h"

int main (int argc, char **argv)
{
  unsigned int rtn;
  
  if (argc < 2) {
    printf ("Expecting a keyword argument\n");
    return 4;
  }
  
  rtn = search_trie (__trie, argv[1]);
  if (rtn == NOT_FOUND)
    printf ("Not Found\n");
  else
    printf ("Value found is %u\n", rtn);
}
