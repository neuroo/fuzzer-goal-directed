extern void __coverage_reach_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_skip_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_enter_func(const unsigned long);
extern void __coverage_exit_func(const unsigned long);
extern void __coverage_kill(const unsigned long);
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
using namespace std;

// This sample program crashes when 4 "AB" sequences are found
// in the supplied string. The idea is for a fuzzer to reconstruct
// the proper sequence.

static const unsigned int MAX_SIZE = 64;
static char buffer[MAX_SIZE] = {0};

void trigger_fault() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(2);
  int *woot_ptr = reinterpret_cast<int *>(trigger_fault);
  *woot_ptr = 0xDEADBEEF;
   __coverage_exit_func(2); return;
}

bool is_b_next(const char *c) {bool __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(7); __coverage_ret_value = (*(c + 1) == 'B'); __coverage_exit_func(7); return __coverage_ret_value; }

bool check_string(const string &input) {bool __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(12);
  int count = 0;
  const char *str = input.c_str();
  int len = 0;
  while (len < input.length() - 1) { __coverage_reach_block(12, __coverage_pred_block, 4); __coverage_pred_block = 4; 
    // state: len < min(len(input), MAX_SIZE)
    const char c = str[len];
    // state: c = input[len];
    switch (c) {
    case 'A': { __coverage_reach_block(12, __coverage_pred_block, 8); __coverage_pred_block = 8; 
      // forward-state: <<is_b_next>>
      // state: c == 'A'

      if (is_b_next(str + len * sizeof(char))) { __coverage_reach_block(12, __coverage_pred_block, 7); __coverage_pred_block = 7; 
        // state := count+=1, len+=1
        len++;
        count++;
      }
      break;
    }
    default: { __coverage_reach_block(12, __coverage_pred_block, 5); __coverage_pred_block = 5; }
    }
    len++;
  }
  __coverage_ret_value = (count == 4); __coverage_exit_func(12); return __coverage_ret_value;
}

int main(int argc, char *argv[]) {int __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(31);
  // forward-state: argc == 1;
  //                len(argv[0]) < MAX_SIZE
  if (argc != 2) { __coverage_reach_block(31, __coverage_pred_block, 6); __coverage_pred_block = 6; 
    cout << "Must have one argument: <parsed string>" << endl;
    __coverage_ret_value = (0); __coverage_exit_func(31); return __coverage_ret_value;
  }
  // state: argc == 1

  const char *arg = argv[1];
  if (strlen(arg) > MAX_SIZE) { __coverage_reach_block(31, __coverage_pred_block, 4); __coverage_pred_block = 4; 
    // state: len(input) >= MAX_SIZE
    cerr << "The given string is too long..." << endl;
    __coverage_ret_value = (0); __coverage_exit_func(31); return __coverage_ret_value;
  }

  // state: len(input) < MAX_SIZE
  string input(argv[1]);
  cout << "Given string: " << input << endl;
  // forward-state: <<check_string>>
  if (check_string(input)) { __coverage_reach_block(31, __coverage_pred_block, 2); __coverage_pred_block = 2; 
    // forward-state: <<trigger_fault>>
    // state: check_string(input) == true
    cout << "SUCCESS" << endl;
    trigger_fault();
    __coverage_ret_value = (1); __coverage_exit_func(31); return __coverage_ret_value;
  }
  __coverage_ret_value = (0); __coverage_exit_func(31); return __coverage_ret_value;
}

//
// Some non-relevant code to test the instrumentation
//

struct Foo {
  int bar;
  int baz;
};

Foo *get1(const Foo &test) {struct Foo * __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(43); __coverage_ret_value = (const_cast<Foo *>(&test)); __coverage_exit_func(43); return __coverage_ret_value; }

Foo &get2(Foo &test, Foo &test2) {unsigned int __coverage_pred_block = 0; __coverage_enter_func(47); // does not work.
  if (&test == &test2)
    { __coverage_reach_block(47, __coverage_pred_block, 2); __coverage_pred_block = 2; struct Foo & __coverage_ret_value_1 = (test); __coverage_exit_func(47); return __coverage_ret_value_1;}
  else
    { __coverage_reach_block(47, __coverage_pred_block, 1); __coverage_pred_block = 1; struct Foo & __coverage_ret_value_2 = (test2); __coverage_exit_func(47); return __coverage_ret_value_2;}
 __coverage_exit_func(47); }
