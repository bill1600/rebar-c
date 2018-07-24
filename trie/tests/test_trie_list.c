/*-----------------------------------------------------------------------------
 * test_trie_list.c
 * 
 * ----------------------------------------------------------------------------
 */

#define _GNU_SOURCE 1
// We need _GNU_SOURCE for our usage of strerror_r

#include "utlist.h"	// from https://github.com/troydhanson/uthash
#include "uthash.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>	// gettimeofday
#include <stdlib.h>	// malloc
#include <stdbool.h>	// true false
#include <errno.h>
#include "search_trie.h"

void show_os_error (const char *msg, int err_code)
{
  char err_buf[100];
  char *err_str = strerror_r (err_code, err_buf, 100);
  printf ("%s\n%s\n", msg, err_str);
}

void show_os_file_error (const char *msg, const char *fname, int err_code)
{
  char err_buf[100];
  char *err_str = strerror_r (err_code, err_buf, 100);
  printf (msg, fname);
  printf ("\n%s\n", err_str);
}

/* ----------- Hash Table structures -------*/
typedef struct key_value_hash_item {
  char *keyword;
  unsigned int value;
  UT_hash_handle hh;
} key_value_hash_item_t;

key_value_hash_item_t *key_value_hash_table = NULL;


/* ---------- Loading input list -----------*/

#define MAX_INLINE 8192

/* ------ Input List structures ---------*/
typedef struct key_value_input {
  char *keyword;
  unsigned int value;
  struct key_value_input *next;
} key_value_input_t;

key_value_input_t *input_list = NULL;

unsigned int input_value = 0;
int input_format = 0; // 2 is keyword value, 1 is keyword without value

key_value_input_t *load_input_line (char *line_ptr)
{
  char keyword[MAX_INLINE];
  int scanf_count;
  key_value_input_t *new_input;

  scanf_count = sscanf (line_ptr, "%8192s %u", keyword, &input_value);
  if ((2 != scanf_count) && (1 != scanf_count)) {
    printf ("Line format error, scan count %d in:\n", scanf_count);
    fputs (line_ptr, stdout);
    return NULL;
  }
  if (input_format != scanf_count) {
    if (input_format == 0) {
      input_format = scanf_count;
    } else {
      printf ("input file format should not change\n");
      return NULL;
    }
  }
  if (1 == scanf_count)
    ++input_value; 
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
  new_input->value = input_value;
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
  input_value = 0;
  input_format = 0;
  
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

int show_input_list (void)
{
  key_value_input_t *item;
  int count;

  LL_COUNT (input_list, item, count);
  printf ("%d keyword/value pairs loaded\n", count);
  LL_FOREACH (input_list, item) {
    printf (" %s %u", item->keyword, item->value);
    if (NULL == item->next)
      printf (" End");
    putchar ('\n');
  }
  return count;
}

/*----------- Building Keyword hash table ------------ */
void free_keyword_hash_table (void)
{
  key_value_hash_item_t *hash_item, *hash_tmp;
  HASH_ITER (hh, key_value_hash_table, hash_item, hash_tmp) {
    HASH_DEL (key_value_hash_table, hash_item);
    free (hash_item);
  }
}

int build_keyword_hash_table (void)
{
  key_value_input_t *input_item;
  key_value_hash_item_t *hash_item;

  key_value_hash_table = NULL;
  LL_FOREACH (input_list, input_item) {
    hash_item = (key_value_hash_item_t *) malloc (sizeof(key_value_hash_item_t));
    if (NULL == hash_item) {
      printf ("memory allocation failed allocating hash table\n");
      if (NULL != key_value_hash_table)
        free_keyword_hash_table ();
      return -1;
    }
    hash_item->keyword = input_item->keyword;
    hash_item->value = input_item->value;
    HASH_ADD_KEYPTR (hh, key_value_hash_table, hash_item->keyword,
      strlen(hash_item->keyword), hash_item);
  }
  printf ("Built keyword hash table\n");
  return 0;
}

/*-----------------------------------------------------*/
char *load_trie_stream (const char *trie_file_name)
{
  FILE * trie_file;
  struct stat file_stat;
  unsigned int trie_file_size;
  size_t trie_bytes_read;
  char *trie_stream;
  
  if (stat (trie_file_name, &file_stat) < 0) {
    show_os_file_error ("Cannot stat file %s", trie_file_name, errno);
    return NULL;
  }
  trie_file_size = (unsigned int) file_stat.st_size;
  printf ("trie stream is %u bytes\n", trie_file_size);
  
  trie_stream = (char *) malloc (trie_file_size);
  if (NULL == trie_stream) {
    printf ("Memory allocation for trie stream failed\n"); 
    return NULL;
  }
  
  trie_file = fopen (trie_file_name, "r");
  if (NULL == trie_file) {
    show_os_file_error ("Error opening trie stream file %s", 
      trie_file_name, errno);
    free (trie_stream);
    return NULL;
  }
  
  trie_bytes_read = fread (trie_stream, 1, (size_t) trie_file_size, trie_file);
  fclose (trie_file);
  if (trie_bytes_read != trie_file_size) {
    printf ("Only %u bytes read of trie_stream\n", (unsigned) trie_bytes_read);
    if (ferror (trie_file)) {
      show_os_file_error ("Error reading file %s", trie_file_name, errno);
    }
    free (trie_stream);
    return NULL;
  }
  return trie_stream;
}

long time_diff (const char *which, struct timeval *start, struct timeval *stop)
{
  long diff_secs;
  if (stop->tv_sec < start->tv_sec) {
    printf ("%s stop secs(%u) < start secs(%u)\n", which,
       (unsigned) stop->tv_sec, (unsigned) start->tv_sec);
    return -1;
  }
  if ((stop->tv_sec == start->tv_sec) && (stop->tv_usec < start->tv_usec)) {
    printf ("%s stop usecs(%u) < start usecs(%u)\n", which,
       (unsigned) stop->tv_usec, (unsigned) start->tv_usec);
    return -1;
  }
  diff_secs = (long) stop->tv_sec - (long) start->tv_sec;
  if (diff_secs > 1000L) {
    printf ("%s seconds diff too great (%ld)\n", which, diff_secs);
    return -1;
  }
  return (1000000L * diff_secs) + (long) stop->tv_usec - (long) start->tv_usec;
}

long null_loop (char *trie_stream, int iterations)
{
  struct timeval start;
  struct timeval stop;
  key_value_input_t *item = NULL;
  unsigned int rtn = 0;
  unsigned int count = 0;
  int i;
  long total_time;

  printf ("Null Loop\n");
  if (gettimeofday (&start, NULL) < 0) {
    show_os_error ("Null loop start gettimeofday error", errno);
    return -1;
  }
  for (i=0; (i<iterations); i++) {
    LL_FOREACH (input_list, item) {
      ++count;
      if (rtn == NOT_FOUND) {
	i = iterations;
	break;
      }
    }
  }  
  if (gettimeofday (&stop, NULL) < 0) {
    show_os_error ("Null loop stop gettimeofday error", errno);
    return -1;
  }
  printf ("Null loop %d iterations %u items\n", iterations, count);
  printf ("Start %u %u, Stop %u %u\n",
    (unsigned) start.tv_sec, (unsigned) start.tv_usec, 
    (unsigned) stop.tv_sec, (unsigned) stop.tv_usec);
  total_time = time_diff ("Null loop", &start, &stop);
  printf ("Total time in null loop %ld usecs\n", total_time);
  return total_time;
}

long hash_timing_loop (char *trie_stream, int iterations)
{
  struct timeval start;
  struct timeval stop;
  key_value_input_t *item = NULL;
  key_value_hash_item_t *hash_item = NULL;
  unsigned int count = 0;
  int i;
  long total_time;

  printf ("Hash Timing Loop\n");  
  if (gettimeofday (&start, NULL) < 0) {
    show_os_error ("Hash Timimg loop start gettimeofday error", errno);
    return -1;
  }
  for (i=0; (i<iterations); i++) {
    LL_FOREACH (input_list, item) {
      ++count;
      HASH_FIND_STR (key_value_hash_table, item->keyword, hash_item);
      if (NULL == hash_item) {
	i = iterations;
	break;
      }
    }
  } 
  if (gettimeofday (&stop, NULL) < 0) {
    show_os_error ("Hash Timing loop stop gettimeofday error", errno);
    return -1;
  }
  printf ("Hash Timing loop %d iterations %u items\n", iterations, count);
/*
  printf ("Start %u %u, Stop %u %u\n",
    (unsigned) start.tv_sec, (unsigned) start.tv_usec, 
    (unsigned) stop.tv_sec, (unsigned) stop.tv_usec);
*/
  if ((NULL == hash_item) && (item != NULL)) {
    printf ("Item %s not found\n", item->keyword);
    return -1;
  } 
  total_time = time_diff ("Hash Timing loop", &start, &stop);
  printf ("Total time in hash timing loop %ld usecs\n", total_time);
  return total_time;
}

long trie_timing_loop (char *trie_stream, int iterations)
{
  struct timeval start;
  struct timeval stop;
  key_value_input_t *item = NULL;
  unsigned int rtn = 0;
  unsigned int count = 0;
  int i;
  long total_time;

  printf ("Trie Timing Loop\n");  
  if (gettimeofday (&start, NULL) < 0) {
    show_os_error ("Trie Timimg loop start gettimeofday error", errno);
    return -1;
  }
  for (i=0; (i<iterations); i++) {
    LL_FOREACH (input_list, item) {
      ++count;
      rtn = search_trie (trie_stream, item->keyword);
      if (rtn == NOT_FOUND) {
	i = iterations;
	break;
      }
    }
  } 
  if (gettimeofday (&stop, NULL) < 0) {
    show_os_error ("Trie Timing loop stop gettimeofday error", errno);
    return -1;
  }
  printf ("Trie Timing loop %d iterations %u items\n", iterations, count);
/*
  printf ("Start %u %u, Stop %u %u\n",
    (unsigned) start.tv_sec, (unsigned) start.tv_usec, 
    (unsigned) stop.tv_sec, (unsigned) stop.tv_usec);
*/
  if ((rtn == NOT_FOUND) && (item != NULL)) {
    printf ("Item %s not found\n", item->keyword);
    return -1;
  } 
  total_time = time_diff ("Trie Timing loop", &start, &stop);
  printf ("Total time in trie timing loop %ld usecs\n", total_time);
  return total_time;
}

void free_all (char *trie_stream)
{
  free_keyword_hash_table ();
  free_input_list ();
  free (trie_stream);
}

int main (int argc, char **argv)
{
  const char *trie_file_name;
  const char *keyword_file_name;
  FILE * keyword_file;
  char *trie_stream;
  int count;
  int iterations = 500;
  long null_loop_time;
  long timing_loop_time, hash_loop_time;
  long call_count, average;
    
  if (argc < 3) {
    printf ("Expecting 2 arguments:\n");
    printf (" 1) trie stream file\n");
    printf (" 2) keyword list file\n");
    return 4;
  }
  
  trie_file_name = argv[1];
  keyword_file_name = argv[2];

  trie_stream = load_trie_stream (trie_file_name);
  if (NULL == trie_stream)
    return 4;
    
  keyword_file = fopen (keyword_file_name, "r");
  if (NULL == keyword_file) {
    show_os_file_error ("Error opening keyword file %s", 
      keyword_file_name, errno);
    free (trie_stream);
    return 4;
  }
  
  count = load_input_list__ (keyword_file);
  printf ("Loaded keywords, count = %d\n", count);
  fclose (keyword_file);
  if (count < 0) {
    free_input_list ();
    free (trie_stream);
    return 4;
  }
  //show_input_list ();
  if (build_keyword_hash_table () != 0) {
    free_input_list ();
    free (trie_stream);
    return 4;
  }

  null_loop_time = null_loop (trie_stream, iterations); 
  if (null_loop_time == -1) {
    free_all (trie_stream);
    return 4; 
  }
  call_count = (long) count * iterations;
  hash_loop_time = hash_timing_loop (trie_stream, iterations);
  if (hash_loop_time != -1) {
    average = (hash_loop_time - null_loop_time) / (long) count;
    printf ("Average time to lookup all names in hash %ld usecs\n", average);
    printf ("Total calls %ld\n", call_count);
    average = (1000L * (hash_loop_time - null_loop_time)) / (long) call_count;
    printf ("Average time per hash search %ld nsecs\n", average);
  }
  
  timing_loop_time = trie_timing_loop (trie_stream, iterations);
  if (timing_loop_time != -1) {
    average = (timing_loop_time - null_loop_time) / (long) count;
    printf ("Average time to lookup all names in trie %ld usecs\n", average);
    printf ("Total calls %ld\n", call_count);
    average = (1000L * (timing_loop_time - null_loop_time)) / (long) call_count;
    printf ("Average time per trie search %ld nsecs\n", average);
  }
  
  free_all (trie_stream);
}
