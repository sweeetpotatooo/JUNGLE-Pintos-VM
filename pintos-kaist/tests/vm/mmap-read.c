/* Uses a memory mapping to read a file. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *actual = (char *) 0x10000000;
  int handle;
  void *map;
  size_t i;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\""); // sample.txt의 버퍼 주소가 커널 영역으로 찍힘. 전체 프로세스가 커널 영역에 생성됐다는 의미인가? 엄청 큰일난 거 안ㄴ
  CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");

  /* Check that data is correct. */
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

  /* Verify that data is followed by zeros. */
  for (i = strlen (sample); i < 4096; i++)
    if (actual[i] != 0)
      fail ("byte %zu of mmap'd region has value %02hhx (should be 0)",
            i, actual[i]);

  munmap (map);
  close (handle);
}
