#include "crc32.h"
#if defined(AMIGA) || defined(AMIGAOS4)
#include <proto/dos.h>
#else
#include <stdio.h>
#endif

int
main(int argc, char** argv)
{
  if (argc != 2) {
#ifdef AMIGAOS4
    IDOS->Printf((APTR)"usage: %s file\n", (int)argv[0]);
#elif defined(AMIGA)
    Printf((APTR)"usage: %s file\n", (int)argv[0]);
#else
    printf("usage: %s file\n", argv[0]);
#endif
    return 1;
  }

  uint32_t crc;
  if (crc32_sum(argv[1], &crc) == 0) {
#ifdef AMIGAOS4
    IDOS->Printf("%lx\n", crc);
#elif defined(AMIGA)
    Printf((APTR)"%lx\n", crc);
#else
    printf("%x\n", crc);
#endif
  } else {
#ifdef AMIGAOS4
    IDOS->Printf("error\n", 0);
#elif defined(AMIGA)
    Printf((APTR)"error\n", 0);
#else
    puts("error\n");
#endif
    return 1;
  }
  return 0;
}
