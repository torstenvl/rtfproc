#ifndef __MEMZERO_H
#define __MEMZERO_H

extern inline void memzero(void *const p, const size_t n)
{
  for (
    volatile unsigned char *volatile p_=(volatile unsigned char *volatile)p;
    p_ < (volatile unsigned char *volatile)(p+n);
    p_++)
  {
      *p_ = 0U;
  }

}

#endif