extern void __coverage_reach_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_skip_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_enter_func(const unsigned long);
extern void __coverage_exit_func(const unsigned long);
extern void __coverage_kill(const unsigned long);
#include <iostream>
#include <stdexcept>
#include <string>
using namespace std;

#define FOO 1

void dump(const char *str) {unsigned int __coverage_pred_block = 0; __coverage_enter_func(54); (void *)(str);  __coverage_exit_func(54); }

void callMe() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(59);
  if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 27); __coverage_pred_block = 27; throw new logic_error("woot");}
  else { __coverage_reach_block(59, __coverage_pred_block, 26); __coverage_pred_block = 26; if (!FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 25); __coverage_pred_block = 25;  __coverage_exit_func(59); return;}}

  switch (FOO) {
  case 0:
     __coverage_reach_block(59, __coverage_pred_block, 24); __coverage_pred_block = 24; callMe();

  case 1:
     __coverage_reach_block(59, __coverage_pred_block, 23); __coverage_pred_block = 23; callMe();
     __coverage_exit_func(59); return;

  default:
     __coverage_reach_block(59, __coverage_pred_block, 22); __coverage_pred_block = 22;  __coverage_exit_func(59); return;
  }

  if (FOO) { __coverage_reach_block(59, __coverage_pred_block, 19); __coverage_pred_block = 19; 
    throw new logic_error("ww");
  }

  if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 17); __coverage_pred_block = 17; throw new logic_error("111");}
  else { __coverage_reach_block(59, __coverage_pred_block, 16); __coverage_pred_block = 16; if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 15); __coverage_pred_block = 15; throw new logic_error("211");}
  else { __coverage_reach_block(59, __coverage_pred_block, 14); __coverage_pred_block = 14; if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 13); __coverage_pred_block = 13; throw new logic_error("212");}
  else { __coverage_reach_block(59, __coverage_pred_block, 12); __coverage_pred_block = 12; if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 11); __coverage_pred_block = 11; throw new logic_error("213");}
  else
    { __coverage_reach_block(59, __coverage_pred_block, 10); __coverage_pred_block = 10; throw new logic_error("214");}}}}

  if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 8); __coverage_pred_block = 8; dump("iff");}
  else { __coverage_reach_block(59, __coverage_pred_block, 7); __coverage_pred_block = 7; if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 6); __coverage_pred_block = 6; dump("else if");}}

  if (FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 4); __coverage_pred_block = 4; dump("aff");}
  else { __coverage_reach_block(59, __coverage_pred_block, 3); __coverage_pred_block = 3; if (!FOO)
    { __coverage_reach_block(59, __coverage_pred_block, 2); __coverage_pred_block = 2; dump("else if");}
  else
    { __coverage_reach_block(59, __coverage_pred_block, 1); __coverage_pred_block = 1;  __coverage_exit_func(59); return;}}
 __coverage_exit_func(59); }

int main(int argc, char *argv[]) {int __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(90);
  try { __coverage_reach_block(90, __coverage_pred_block, 5); __coverage_pred_block = 5; 
    callMe();
  } catch (const exception &e) { __coverage_reach_block(90, __coverage_pred_block, 3); __coverage_pred_block = 3; 
    cerr << "except: " << e.what() << endl;
  } catch (...) { __coverage_reach_block(90, __coverage_pred_block, 4); __coverage_pred_block = 4; 
    throw new logic_error("ex");
  }
  __coverage_ret_value = (0); __coverage_exit_func(90); return __coverage_ret_value;
}
