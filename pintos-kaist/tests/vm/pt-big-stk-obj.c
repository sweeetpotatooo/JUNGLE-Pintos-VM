/* Allocates and writes to a 64 kB object on the stack.
   This must succeed. */

#include <string.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char stk_obj[65536]; // 이러면 rsp는 그냥 바로 처 내려감. 중간에 페이지 할당은 이 시점에 일어나지도 않음
  struct arc4 arc4;

  arc4_init (&arc4, "foobar", 6);
  memset (stk_obj, 0, sizeof stk_obj); // 메모리 셋을 거꾸로 올라가면서 함. 그래서 페이지 폴트는 rsp-8보다 위에 지점에서 계속 나게 됨.(fault_addr > rsp-8)
  // 만약에 rsp를 내림과 동시에 폴트가 발생한다면? 그러면 fault_addr == rsp-8.
  // 이 두 조건을 모두 합치면 fault_addr >= rsp - 8이다.
  // rsp는 저만치 아래 있는데 그 중간에서 폴트가 날 수도 있고, 그 상황에도 스택 성장을 시켜 줘야 함!
  arc4_crypt (&arc4, stk_obj, sizeof stk_obj);
  msg ("cksum: %lu", cksum (stk_obj, sizeof stk_obj));
}
