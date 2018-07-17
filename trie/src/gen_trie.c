/*-----------------------------------------------------------------------------
 * gen_trie.c
 * 
 * ----------------------------------------------------------------------------
 */

#define _GNU_SOURCE 1
// We need _GNU_SOURCE for our usage of strerror_r

#include "utlist.h"	// from https://github.com/troydhanson/uthash
#include <stdio.h>
#include <string.h>
#include <stdlib.h>	// malloc
#include <stdbool.h>	// true false
#include <errno.h>


/* ------ Input List structures ---------*/
typedef struct key_value_input {
  char *keyword;
  unsigned int value;
  struct key_value_input *next;
} key_value_input_t;

key_value_input_t *input_list = NULL;

/* ------- Internal Trie structures --------*/
struct word_part_node;

typedef struct word_part_list {
  struct word_part_node *head;
} word_part_list_t;

#define NO_VALUE ((unsigned int) (-1))

typedef struct word_part_node {
  char *seq;
  unsigned int seq_len;
  unsigned int byte_offset;
  unsigned int value;
  struct word_part_list *down;
  struct word_part_node *next;
} word_part_node_t;

struct word_part_list * itrie_top;


/* --- Return values for build_itrie ------*/
#define BUILD_ITRIE_SUCCESS 0
#define BUILD_ITRIE_ALREADY_IN_TRIE 1
#define BUILD_ITRIE_MEM_FAIL -1

void show_os_error (const char *msg, int err_code)
{
  char err_buf[100];
  char *err_str = strerror_r (err_code, err_buf, 100);
  printf ("%s\n%s\n", msg, err_str);
}


/* ---------- Loading input list -----------*/

#define MAX_INLINE 8192

key_value_input_t *load_input_line (char *line_ptr)
{
  char keyword[MAX_INLINE];
  int scanf_count;
  unsigned value;
  key_value_input_t *new_input;

  scanf_count = sscanf (line_ptr, "%8192s %u", keyword, &value);
  if (2 != scanf_count) {
    printf ("Line format error, scan count %d in:\n", scanf_count);
    fputs (line_ptr, stdout);
    return NULL;
  }
  new_input = (key_value_input_t *) malloc (sizeof(key_value_input_t));
  if (NULL == new_input) {
    printf ("memory allocation failed (1) reading keyword input file\n");
    return NULL;
  }
  new_input->keyword = (char *) malloc (strlen(keyword) + 1);
  if (NULL == new_input->keyword) {
    free (new_input);
    printf ("memory allocation failed (2) reading keyword input file\n");
    return NULL;
  }
  strcpy (new_input->keyword, keyword);
  new_input->value = value;
  new_input->next = NULL;
  return new_input;
}

int cmp_key_value_struct (key_value_input_t *kv1, key_value_input_t *kv2)
{
  return strcmp (kv1->keyword, kv2->keyword);
}

int load_input_list__ (FILE *ifile)
{
  char in_line[MAX_INLINE];
  char *line_ptr;
  struct key_value_input *new_input;
  int line_count = 0;
  size_t white_span;
  
  input_list = NULL;
  while (true) {
    line_ptr = fgets (in_line, MAX_INLINE, ifile);
    if (NULL == line_ptr) {
      if (0 == ferror (ifile))
        return line_count;
      show_os_error ("Error reading keyword input file", errno);
      return -1;
    }
    white_span = strspn (line_ptr, " \t");
    if ((line_ptr[white_span] == '\0') || (line_ptr[white_span] == '\n'))
      continue;  // skip blank line
    new_input = load_input_line (line_ptr+white_span);
    if (NULL == new_input) {
      return -1;
    }
    //LL_INSERT_INORDER (input_list, new_input, cmp_key_value_struct);
    LL_APPEND (input_list, new_input);
    line_count++;
  }
}

void free_input_list (void)
{
  key_value_input_t *item;
  key_value_input_t *temp_item;
  
  LL_FOREACH_SAFE (input_list, item, temp_item) {
    free (item->keyword);
    free (item);
  }
}

int load_input_list (const char *ifile_name)
{
  int rtn;
  FILE *ifile = fopen (ifile_name, "r");
  
  if (NULL == ifile) {
    show_os_error ("Error opening keyword input file", errno);
    return -2;
  }
  rtn = load_input_list__ (ifile);
  fclose (ifile);
  if (rtn < 0)
    free_input_list ();
  return rtn;
}


/* ------------ Building internal trie -----------*/
int max_stack_depth = 0;
int current_stack_depth = 0;

struct word_part_node *create_word_part_node (
  char *seq, unsigned int seq_len, unsigned int value)
{
  struct word_part_node *new_node = (struct word_part_node *)
    malloc (sizeof(struct word_part_node));
  if (NULL == new_node) {
    printf ("memory allocation failed creating new word_part node\n");
    return NULL;
  }
  new_node->seq = seq;
  new_node->seq_len = seq_len;
  new_node->value = value;
  new_node->byte_offset = 0;
  new_node->down = NULL;
  new_node->next = NULL;
  return new_node;
}	  

struct word_part_list *create_word_part_list__ (struct word_part_node *first_word)
{
  struct word_part_list *new_list = (struct word_part_list *)
    malloc (sizeof(struct word_part_list));
  if (NULL == new_list) {
    printf ("memory allocation failed creating new word_part list\n");
    return NULL;
  }
  new_list->head = NULL;
  if (NULL != first_word) {
    LL_APPEND (new_list->head, first_word);
  }
  ++current_stack_depth;
  if (current_stack_depth > max_stack_depth)
    max_stack_depth = current_stack_depth;
  return new_list;  
}

struct word_part_list *create_word_part_list (char *first_word_seq,
  unsigned int first_word_len, unsigned int first_word_value)
{
  struct word_part_node *first_word = NULL;
  if (NULL != first_word_seq) {
    first_word = create_word_part_node (first_word_seq, first_word_len,
      first_word_value);
    if (NULL == first_word)
      return NULL;
  }
  return create_word_part_list__ (first_word);
}

struct word_part_list *itrie_split_word_part_node (
  struct word_part_node *word_part, unsigned int offset)
{ 
  struct word_part_list *new_list = create_word_part_list (
    word_part->seq+offset, word_part->seq_len-offset, word_part->value);
  if (NULL == new_list)
    return NULL;
  new_list->head->down = word_part->down;
  word_part->seq_len = offset;
  word_part->down = new_list;
  word_part->value = NO_VALUE;
  return new_list;
}

struct word_part_node *itrie_find_first_char (
  char first_char, struct word_part_list *current_list)
{
  struct word_part_node *word_part;
  LL_FOREACH (current_list->head, word_part) {
    if (first_char == word_part->seq[0])
      return word_part;
    if (first_char < word_part->seq[0])
      return NULL;
  }
  return NULL;
}
  
int cmp_word_part_struct (word_part_node_t *word1, word_part_node_t *word2)
{
  return strcmp (word1->seq, word2->seq);
}

unsigned int itrie_match_remaining (char *word1, unsigned int word1_len,
  char *word2, unsigned int word2_len)
{
  unsigned int i;

  /* start with 1 because first char is already matched */  
  for (i=1; (i<word1_len) && (i<word2_len); i++)
    if (word1[i] != word2[i])
      return i;
  return i;
}

int itrie_add_word (char *word, unsigned int value)
{  
  unsigned int word_len = (unsigned int) strlen (word);
  unsigned int matched_so_far = 0;
  struct word_part_list * current_list = itrie_top;

  current_stack_depth = 1;
    
  while (true) {
    unsigned int match_len;
    struct word_part_node *found_node = 
      itrie_find_first_char (word[matched_so_far], current_list);
    struct word_part_node *new_word_part;

    if (NULL == found_node) {
      new_word_part = create_word_part_node (
        word+matched_so_far, word_len-matched_so_far, value);
      LL_INSERT_INORDER (current_list->head, new_word_part, cmp_word_part_struct);
      break;
    }
    match_len = itrie_match_remaining (word+matched_so_far, word_len-matched_so_far, 
      found_node->seq, found_node->seq_len);
    
    /* first case: all of remainng search word matches all of word_part_node */
    if ((match_len == (word_len-matched_so_far)) &&
        (match_len == found_node->seq_len))
    { 
      if (found_node->value == NO_VALUE) {
	found_node->value = value;
	break;
      }
      return BUILD_ITRIE_ALREADY_IN_TRIE;
    }
 
    /* second case: all of remaining search word matched front of word_part_mode */
    if (match_len == (word_len-matched_so_far))
    {
      struct word_part_list *down_list = 
        itrie_split_word_part_node (found_node, match_len);
      if (NULL == down_list)
        return BUILD_ITRIE_MEM_FAIL;
      found_node->value = value;
      break;
    }


    /* third case: front of remaining search word matched all of word_part_mode */
    if (match_len == found_node->seq_len)
    {
      matched_so_far += match_len;
      if (NULL != found_node->down) {
	current_list = found_node->down;
	continue;
      }
      found_node->down = create_word_part_list (
        word+matched_so_far, word_len-matched_so_far, value);
      if (NULL == found_node->down)
        return BUILD_ITRIE_MEM_FAIL;
      break;
    }
   
    /* last case: front of remaining search word matched front of word_part_node */
    {
      struct word_part_list *down_list = 
        itrie_split_word_part_node (found_node, match_len);
      if (NULL == down_list)
        return BUILD_ITRIE_MEM_FAIL;
      matched_so_far += match_len;
      new_word_part = create_word_part_node (
        word+matched_so_far, word_len-matched_so_far, value);
      if (NULL == new_word_part)
        return BUILD_ITRIE_MEM_FAIL;
      LL_INSERT_INORDER (down_list->head, new_word_part, cmp_word_part_struct);
      break;
    }	

  }
  
  return BUILD_ITRIE_SUCCESS;
}

int build_itrie (void)
{
  key_value_input_t *input_item;
  itrie_top = create_word_part_list (NULL, 0,0);
  if (NULL == itrie_top)
    return BUILD_ITRIE_MEM_FAIL;

  max_stack_depth = 1;
  LL_FOREACH (input_list, input_item) {
    int rtn = itrie_add_word (input_item->keyword, input_item->value);
    if (rtn != 0)
      return rtn;
  }
  printf ("Build itrie, max_stack_depth %d\n", max_stack_depth);
  return BUILD_ITRIE_SUCCESS;
    
}

/* ------------ Freeing internal trie -----------*/
void free_itrie (void)
{
  struct word_part_node *first_node;
  struct word_part_list *stack[max_stack_depth];  

  if (NULL == itrie_top)
    return;
    
  stack[0] = itrie_top;
  current_stack_depth = 0;
  
  while (current_stack_depth >= 0)
  {
    if (NULL != stack[current_stack_depth]->head)
    {
      first_node = stack[current_stack_depth]->head;
      stack[current_stack_depth]->head = first_node->next;
      if (NULL != first_node->down) {
	stack[++current_stack_depth] = first_node->down;
      }
      free (first_node);
      continue;
    }
    free (stack[current_stack_depth--]);
  }
}

/* ------------ Displayng internal trie -----------*/
void display_indent (int indent_level)
{
  int i;
  for (i=0; i<(indent_level*2); i++)
    putchar (' ');
}

void display_itrie_index (struct word_part_list *index, int indent_level)
{
  struct word_part_node *word_part;
  display_indent (indent_level);
  printf ("Level %d index: ", indent_level);
  if (NULL == index->head) {
    printf ("NULL\n");
    return;
  }
  LL_FOREACH (index->head, word_part) {
    putchar (word_part->seq[0]);
  }
  putchar ('\n');  
}

void display_itrie_node (struct word_part_node *word_part, int indent_level)
{
  int i;
  display_indent (indent_level);
  printf ("Level %d node: ", indent_level);
  for (i=0; i<word_part->seq_len; i++)
    putchar (word_part->seq[i]);
  printf (" byte offset: %u", word_part->byte_offset);
  if (word_part->value == NO_VALUE)
    printf (" value: NONE");
  else
    printf (" value: %u", word_part->value);
  if (NULL != word_part->down)
    printf (" D");
  putchar ('\n');
}
  
void display_itrie (void)
{
  struct word_part_node *current_node;
  struct word_part_node *stack[max_stack_depth];
  
  if (NULL == itrie_top) {
    printf ("Empty internal trie\n");
    return;
  }
  
  current_node = itrie_top->head;
  if (NULL == current_node) {
    printf ("Empty internal trie\n");
    return;
  }
  
  stack[0] = current_node;
  current_stack_depth = 0;
  display_itrie_index (itrie_top, 0);
  
  while (true)
  {
    display_itrie_node (current_node, current_stack_depth);
    if (NULL != current_node->down) {
      struct word_part_list *down_list = current_node->down;
      if (NULL != down_list->head) {
        ++current_stack_depth;
        display_itrie_index (down_list, current_stack_depth);
        current_node = down_list->head;
        stack[current_stack_depth] = current_node;
        continue;
      }
    }
    while (true) {
      if (NULL != current_node->next) {
        current_node = current_node->next;
        stack[current_stack_depth] = current_node;
        break;
      }
      --current_stack_depth;
      if (current_stack_depth < 0)
        return;  // We are done
      current_node = stack[current_stack_depth];
    }
  }

	
}

/* ------- Inserting byte offsets into nodes of internal trie  -----*/
unsigned int index_size (struct word_part_node *first_word_of_list)
{
  int num_words;
  struct word_part_node *word_part;
  
  LL_COUNT (first_word_of_list, word_part, num_words);
  return (unsigned int) (6 * num_words) + 1;
}

void insert_positions_in_itrie (void)
{
  unsigned int current_pos = 0;
  struct word_part_node *current_node;
  struct word_part_node *stack[max_stack_depth];
  
  if (NULL == itrie_top) {
    printf ("Empty internal trie\n");
    return;
  }
  
  current_node = itrie_top->head;
  if (NULL == current_node) {
    printf ("Empty internal trie\n");
    return;
  }
  
  stack[0] = current_node;
  current_stack_depth = 0;
  current_pos = index_size (current_node);
  
  while (true)
  {
    current_node->byte_offset = current_pos;
    current_pos += (current_node->seq_len-1);
    if (current_node->value != NO_VALUE)
      current_pos += 2;
      
    if (NULL != current_node->down) {
      struct word_part_list *down_list = current_node->down;
      if (NULL != down_list->head) {
        ++current_stack_depth;
        current_pos += index_size (down_list->head);
        current_node = down_list->head;
        stack[current_stack_depth] = current_node;
        continue;
      }
    }
    while (true) {
      if (NULL != current_node->next) {
        current_node = current_node->next;
        stack[current_stack_depth] = current_node;
        break;
      }
      --current_stack_depth;
      if (current_stack_depth < 0)
        return;  // We are done
      current_node = stack[current_stack_depth];
    }
  }

	
}

/* ----------- Write Output --------------*/ 
int byte_output_count = 0;

void write_separator (FILE *hfile)
{
  if (0 == byte_output_count) {
    fprintf (hfile, "  ");
    return;
  }
  if (0 == (byte_output_count & 15)) {
    fprintf (hfile, ",\n  ");
    return;
  }
  fputc (',', hfile);
}

void write_byte (int obyte, bool numeric, FILE *ofile, FILE *hfile)
{
  obyte &= 0xFF;
  if (NULL != hfile) {
    write_separator (hfile);
    if (numeric || (obyte < 32) || (obyte > 127))
      fprintf (hfile, "0x%02X", obyte);
    else {
      fputc ('\'', hfile);
      fputc (obyte, hfile);
      fputc ('\'', hfile);
    }
  }
  if (NULL != ofile) {
    fputc (obyte, ofile);
  }
  ++byte_output_count;
}

void write_index (struct word_part_list *index, FILE *ofile, FILE *hfile)
{
  int index_len, word_len;
  int obyte[3];
  struct word_part_node *word_part;

  if (NULL == index->head)
    return;  
  LL_COUNT (index->head, word_part, index_len);
  write_byte (index_len, true, ofile, hfile);
  LL_FOREACH (index->head, word_part) {
    /* ---- write 6 bytes ---- */
    write_byte (word_part->seq[0], false, ofile, hfile);
    obyte[2] = word_part->byte_offset & 0xFF;
    obyte[1] = (word_part->byte_offset >> 8);
    obyte[0] = (word_part->byte_offset >> 16);
    if (word_part->value != NO_VALUE)
      obyte[0] |= 0x80;
    write_byte (obyte[0], true, ofile, hfile);
    write_byte (obyte[1], true, ofile, hfile);
    write_byte (obyte[2], true, ofile, hfile);
    word_len = word_part->seq_len - 1;
    obyte[1] = word_len & 0xFF;
    obyte[0] = (word_len >> 8);
    if (NULL != word_part->down)
      obyte[0] |= 0x80;
    write_byte (obyte[0], true, ofile, hfile);
    write_byte (obyte[1], true, ofile, hfile);
  }
}

void write_output__ (FILE *ofile, FILE *hfile)
{
  struct word_part_node *current_node;
  struct word_part_node *stack[max_stack_depth];
  
  if (NULL == itrie_top) {
    printf ("Empty internal trie\n");
    return;
  }
  
  current_node = itrie_top->head;
  if (NULL == current_node) {
    printf ("Empty internal trie\n");
    return;
  }

  byte_output_count = 0;  
  stack[0] = current_node;
  current_stack_depth = 0;
  if (NULL != hfile) {
    fprintf (hfile, "static const char __trie[] = {\n");
  }
  write_index (itrie_top, ofile, hfile);
  
  while (true)
  {
    int i;
    
    /* start with 1 because first char is in the index */
    for (i=1; i<current_node->seq_len; i++)
      write_byte (current_node->seq[i], false, ofile, hfile);
    if (current_node->value != NO_VALUE) {
      write_byte ((current_node->value >> 8) & 0xFF, true, ofile, hfile);
      write_byte (current_node->value & 0xFF, true, ofile, hfile);
    }
      
    if (NULL != current_node->down) {
      struct word_part_list *down_list = current_node->down;
      if (NULL != down_list->head) {
        ++current_stack_depth;
        write_index (down_list, ofile, hfile);
        current_node = down_list->head;
        stack[current_stack_depth] = current_node;
        continue;
      }
    }
    while (true) {
      if (NULL != current_node->next) {
        current_node = current_node->next;
        stack[current_stack_depth] = current_node;
        break;
      }
      --current_stack_depth;
      if (current_stack_depth < 0) {
        if (NULL != hfile)
          fprintf (hfile, "\n};\n");
        return;  // We are done
      }
      current_node = stack[current_stack_depth];
    }
  }

}

int write_output (const char *ofile_name, const char *hfile_name)
{
  FILE *ofile = NULL;
  FILE *hfile = NULL;
  
  if (NULL != ofile_name) {
    ofile = fopen (ofile_name, "w");
    if (NULL == ofile) {
      show_os_error ("Error opening output byte file", errno);
      return -2;
    }
  }
  if (NULL != hfile_name) {
    hfile = fopen (hfile_name, "w");
    if (NULL == hfile) {
      show_os_error ("Error opening output .h file", errno);
      if (NULL != ofile)
        fclose (ofile);
      return -2;
    }
  }
  
  write_output__ (ofile, hfile);
  printf ("%d bytes written to output file %s\n",
    byte_output_count, ofile_name);
  if (NULL != ofile)
    fclose (ofile);
  if (NULL != hfile)
    fclose (hfile);
  return 0;
}

/* -------------------------------------------------------*/

int load_and_show_input_list (const char *ifile_name, bool show)
{
  key_value_input_t *item;
  int rtn, count;

  rtn = load_input_list (ifile_name);
  printf ("load input list, rtn = %d\n", rtn);
  if (rtn < 0)
    return rtn;
  LL_COUNT (input_list, item, count);
  printf ("%d keyword/value pairs loaded\n", count);
  if (!show)
    return rtn;
  LL_FOREACH (input_list, item) {
    printf (" %s %u", item->keyword, item->value);
    if (NULL == item->next)
      printf (" End");
    putchar ('\n');
  }
  return rtn;
}

void usage (void)
{
  printf ("Usage:\n");
  printf (" gen_trie [optional options] input-file "
          "[optional hfile] [optional dat_file]\n");
  printf ("Where\n");
  printf ("hfile ends with .h and dat_file ends with .dat\n");
}
    
int main (int argc, char **argv)
{
  bool show_input_opt = false;
  bool show_itrie_opt = false;
  char *ifile_name = NULL;
  char *hfile_name = NULL;
  char *dat_file_name = NULL;
  int i, rtn;

  if (argc < 2) {
    usage ();
    return 4;
  }
  
  for (i=1; i<argc; i++) {
    char *arg = argv[i];
    char *dot_pos;
    if (arg[0] == '-') {
      if (NULL != strchr (arg+1, 'i'))
        show_input_opt = true;
      if (NULL != strchr (arg+1, 't'))
        show_itrie_opt = true;
      continue;
    }
    dot_pos = strchr (arg, '.');
    if (NULL != dot_pos) {
      if (strcmp (dot_pos+1, "h") == 0) {
        hfile_name = arg;
        continue;
      }
      if (strcmp (dot_pos+1, "dat") == 0) {
        dat_file_name = arg;
        continue;
      }
    }

    if (NULL == ifile_name)
      ifile_name = arg;
    else {
	printf ("Input file specified twice: %s and %s\n",
	  ifile_name, arg);
	return 4;
    }
  }
	    
  if (NULL == ifile_name) {
    printf ("Missing inut file name\n\n");
    usage ();
    return 4;
  }
  if (load_and_show_input_list (ifile_name, show_input_opt) < 0)
    return 4;
  rtn = build_itrie ();
  printf ("Build itrie, rtn = %d\n", rtn);
  if (rtn < 0)
    return 4;
  insert_positions_in_itrie ();
  if (show_itrie_opt)
    display_itrie ();
    
  if ((NULL != dat_file_name) || (NULL != hfile_name))
    write_output (dat_file_name, hfile_name);
  
  free_itrie ();
  free_input_list ();
}
