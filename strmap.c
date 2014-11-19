#include <stdlib.h>

int
str_cmp(void *a1, void *a2)
{
	return strcmp((char *)a1, (char *)a2);
}

size_t
str_hash(void *a)
{
	char *p = a;
	size_t hval;

	/* Who doesn't like a bit of magic? */
#ifdef __amd64__
	hval = 0xcbf29ce484222325ULL;
#else
	hval = 0x811c9dc5;
#endif

	while (*p != '\0') {
		hval ^= (size_t) *p;
#ifdef __amd64__
		/* 64 bit version. */
		hval += ((hval << 1) + (hval << 4) + (hval << 5) +
			 (hval << 7) + (hval << 8) + (hval << 40));
#else
		/* 32 bit version. */
		hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
#endif
		p++;
	}

	/* return our new hash value */
	return hval;
}
