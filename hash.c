/*
 * $Id: hash.c,v 1.1 2004/10/11 00:38:57 jutta Exp $
 *
 * $Log: hash.c,v $
 * Revision 1.1  2004/10/11 00:38:57  jutta
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "abnfgenp.h"

static char const rcsid[] = 
  "$Id: hash.c,v 1.1 2004/10/11 00:38:57 jutta Exp $";

/*  hash.c -- A binary hashtable.
 * 
 *  Maps octet strings to objects.  All objects have the same size;
 *  the keys can have arbitrary sizes.  The objects can be allocated
 *  and retrieved, traversed in some random, but complete order;
 *  given the pointer to an object, its key can be retrieved in
 *  constant time.
 */

typedef struct slot {

	/* application data, possibly of size 0, sits here. 
	 */

	void		* next;		/* collision chain		   */
	unsigned long	  hash;		/* hash value of name		   */
	size_t		  size;		/* size of application byte string */

	/* {size} bytes of hash value sit here.
	 */

} SLOT, * Slot;

/*  In the macros below, {base} points to the start of the
 *  (constant-, but run-time-sized) application data area;
 *  {h} is the hashtable.
 */
#define	INFO(h, base)	((Slot)((char *)(base) + (h)->o))
#define NEXT(h, base)	(INFO(h, base)->next)
#define SIZE(h, base)	(INFO(h, base)->size)
#define MEM( h, base)	((void *)((char *)(base) + (h)->o + sizeof(SLOT)))

/*  Turn an array of {n} bytes into an unsigned long.
 *  I'm using three different policies, dependent on the size:
 *  for short values, simply copy them;
 *  for medium-sized, shift and add;
 *  for large values, just add up the bytes (since I don't want to
 * 		end up with just the last 32 bytes influencing the value.)
 */
static unsigned long hashf(unsigned char const * mem, size_t size)
{
	register unsigned long i = 0;

	while (size--) i = i * 33 + *mem++;
	return i;
}

/*  The power of 2 that is greater or equal to {size}.
 */
static unsigned long hash_round(unsigned long size)
{
	unsigned long i;

	if (size >= 1) size += size - 1;
	for (i = 1; i && i < size; i <<= 1) ;

	return i;
}

/*  Given the size of the static application data <elsize>
 *  and the initial chunk size of the table <m>, initialize a
 *  hashtable object and allocate the first page that will
 *  be given out later.
 *
 *  Returns
 *
 *	 0 	success
 *	-1	allocation failed.
 *
 *  This function is used by the hashtable create function,
 *  and by methods of objects that inherit from a hashtable;
 *  plain users just use htable() or hashtable().
 */
int hashinit(hashtable * t, size_t elsize, int m)
{
	/*  Pad the element size to multiples of sizeof(SLOT)
	 */
	t->o = ((elsize + (sizeof(SLOT) - 1)) / sizeof(SLOT)) * sizeof(SLOT);

	/*  Round the chunk size until table selection and indexing become
	 *  simple masking steps; initialize housekeeping information.
	 */
	t->m    = m = hash_round((unsigned long)m);
	t->mask = m - 1; 		/* mask index in a table 	*/
	t->l    = m * 2 / 3;		/* resize table when t->l <= 0 	*/ 
	t->n    = 0;			/* # of allocated elements.	*/

	/* Create the actual table of buckets.
	 */
	{ 	register void   ** c, ** e;

		if (!(c = t->h = TMALLOC(void *, m))) return -1;
		for (e = c + m; c < e; *c++ = (void *)0)
			;
	}
	return 0;
}

/*  Given the size of the static application data <elsize>
 *  and the initial chunk size of the table <m>, allocate and
 *  initialize a hashtable object.
 */
hashtable * hashcreate(size_t elsize, int m)
{
	hashtable * t;

	if (  !(t = TMALLOC(hashtable, 1))
	   || hashinit(t, elsize, m)) {

		if (t) VFREE(t);
		return (hashtable *)0;
	}
	return t;
}

/*  Double the number of slots available in a hashtable.
 */
int hashgrow(hashtable * h)
{
	{ void ** c;
	  if (!(c = TREALLOC(void *, h->h, h->m * 2))) {
		return -1;
	  }
	  h->h = c;
	}

	/*  Move entries into new chain slots if their hash value
	 *  has the new bit set.
	 */
	{ register unsigned long const newbit = h->m;
	  register void ** 	 o = (void **)h->h,
			** 	 n = o + h->m,
			** const N = n + h->m,
			** ne, ** p;

	  /* 	|--- old buckets ---|--- new buckets ---|
	   *    o >>>               n >>>               N
	   */
	  for (; n < N; n++, o++) {

		ne = n;	 /*  ne points to the current end of the new chain */ 
		p  = o;  /*  p  walks the old chain.			   */

		while (*p)
			if (INFO(h, *p)->hash & newbit) {

				/* make *p skip an item; chain that item
				 * into *ne instead; make the pointer to *p
				 * point to next of *p.  (Pull on the rope.)
				 */

				* ne = * p;
				ne   = &NEXT(h, *ne);
				* p  = * ne;
			}
			else p = &NEXT(h, *p);	/* move along the rope */

		/*
		 * Terminate the new chain.  (*p inherits the old terminator.)
		 */
		*ne = (void *)0;
	  }
	}

	h->mask |= h->m;
	h->m    *= 2;
	h->l     = h->m * 2 / 3;	/* recalculate limit	*/

	return 0;
}

/*  Hash the key <mem, size> into the table.
 *  If it exists, return
 *  	if alloc is 2, a null pointer;
 * 	otherwise a pointer to it;
 *  if it doesn't exist,
 * 	if alloc is 1, allocate a new element and return a pointer to that;
 * 	else, just return a null pointer.
 *
 *  The pointers returned point to the constant-size data storage, not
 *  to the hashed name; however, the name can be derived from the pointer
 *  to the storage in constant, short, time (using hashname).
 */
void * hash(hashtable * h, void const * mem, size_t size, int alloc)
	/* alloc: 0: readonly;  1: read or create; 2: create */
{
	unsigned long i = hashf((unsigned char *)mem, size);
	void	   ** s;
	void	    * e;
	Slot	      slot;

	for (s = h->h + (i & h->mask); *s; s = &slot->next) {
		slot = INFO(h, *s);
		if (slot->hash >= i) {
			if (slot->hash > i) break;
			else if (  SIZE(h, *s) == size
				&& !memcmp(MEM(h, *s), mem, SIZE(h, *s))) {

				if (alloc == 2) {
					return (char *)0;
				}
				return *s;
			}
		}
	}
	if (!alloc) {
		return (char *)0;
	}

	if (!(e = malloc(h->o + sizeof(SLOT) + size + 1))) {
		return (void *)0;
	}

	memset(e, 0, h->o);			/* user data 	*/
	memcpy(MEM(h, e), mem, size);
	((char *)MEM(h, e))[size] = 0;
	slot = INFO(h, e);
	slot->size = size; 
	slot->hash = i;

	/* chain in
	 */
	slot->next = *s;
	*s = e;

	/* count and do housekeeping if necessary.
	 */
	h->n++;
	if (h->n >= h->l) hashgrow(h);

	return e;
}

/*
 * Repeated calls to hashnext, using the previous result
 * as a second argument, step through all filled slots in the table.
 * (And can hence be used to simply dump the table).
 *
 *	char * p = 0; extern hashtable * h;
 *	while (p = hashnext(h, p)) ...
 *
 * To retrieve the first element, use a null pointer as the second argument.
 * After the last element in the table has been returned, hashnext()
 * returns a null pointer.
 */
void * hashnext(hashtable * h, void const * p)
{
	assert(h);
	if (p && NEXT(h, p)) return NEXT(h, p);		/* same bucket chain */
	else {
		register void ** s = h->h, ** const e = s + h->m;

		/* linear scan for next bucket.
		 */
		if (p) s += (INFO(h, p)->hash & h->mask) + 1;
		while (s < e)
			if (*s++) return s[-1];
		return (void *)0;
	}
}

/*  Destroy a hashtable.
 *
 *  This function is destined to be used by the hashtable destroy
 *  function and by methods of objects that inherit from a hashtable;
 *  plain users just call hdestroy() or hashdestroy().
 */
void hashfinish(hashtable * h)
{
	assert(h);

	{ register void ** s = h->h, ** const e = s + h->m;

	  while (s < e) {
		register char * n = *s++, * p;
		while (p = n) {
			n = NEXT(h, p);
			VFREE(p);
		}
	  }
	}
	VFREE(h->h);
}

/*  Destroy a hashtable and free the hashtable object itself.
 */
void hashdestroy(hashtable * h)
{
	assert(h);

	hashfinish(h);
	VFREE(h);
}


/*  Given a user-data pointer, return a pointer to the storage of the `name'.
 */
void const * hashmem(hashtable * h, void const * ob)
{
	assert(ob);
	assert(h);
	return MEM(h, ob);
}

/*  Given a user-data pointer, return the size of the `name'.
 */
size_t hashsize(hashtable * h, void const * ob) {

	assert(ob);
	assert(h);
	return SIZE(h, ob);
}

/*
 *  Delete a single entry from the hashtable.
 */
void hashdelete(hashtable * h, void * ob)
{
	assert(h);
	assert(ob);

	{ void ** s = h->h + (h->mask & INFO(h, ob)->hash);
	  while (*s != ob) {
		assert(*s);
		s = &NEXT(h, *s);
	  }
	  *s = NEXT(h, ob);
	}

	VFREE(ob);
	h->n--;
}
