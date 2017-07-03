/* Hash tables
 * 15-122 Principles of Imperative Computation, Fall 2010
 * Frank Pfenning
 */

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include "map.h"

typedef void* ht_key;
typedef void* ht_elem;
 
/* Hash table interface */
typedef struct ht* ht;
ht ht_new (int init_size,
	   ht_key (*elem_key)(ht_elem e),
	   bool (*equal)(ht_key k1, ht_key k2),
	   int (*hash)(ht_key k, int m)
	   );
void ht_insert(ht H, ht_elem e);
ht_elem ht_search(ht H, ht_key k);
void ht_free(ht H, void (*elem_free)(ht_elem e));

#endif
