# trie

C Implementation of trie generation and lookup

Description is in file description.txt

       
# -------- Build Information -----------
 
 generator program is gen_trie.c  
 it requires utlist.h  
 
 build it with  
 
 gcc -D_GNU_SOURCE -o gen_trie gen_trie.c -lm  
 
 the option _GNU_SOURCE is needed to support the usage of strerror_r.  

 
# -------- Run Information ----------

gen_trie [optioms] <input_file.txt> [optional byte stream file] [optional .h file]  

options are:  

-i  display the input keyword pair list after loading it  
-t  display the internal trie after building it.  

input file is a list of keyword/value pairs, like this:  

APPLE 0  
BAD 1  
BAKER 2  
BAKERY 3  
BAKES 4  
BALL 5  
BALLOON 6  
BALLOT 7  
BALLS 8  
CANDY 9  

byte stream file must have extension ".dat".  
It's just the bytes of the trie.  

.h file must have the extension ".h"  

