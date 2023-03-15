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

void trigger_fault() {
  int *woot_ptr = reinterpret_cast<int *>(trigger_fault);
  *woot_ptr = 0xDEADBEEF;
  return;
}

bool is_b_next(const char *c) { return *(c + 1) == 'B'; }

bool check_string(const string &input) {
  int count = 0;
  const char *str = input.c_str();
  int len = 0;
  while (len < input.length() - 1) {
    // state: len < min(len(input), MAX_SIZE)
    const char c = str[len];
    // state: c = input[len];
    switch (c) {
    case 'A': {
      // forward-state: <<is_b_next>>
      // state: c == 'A'

      if (is_b_next(str + len * sizeof(char))) {
        // state := count+=1, len+=1
        len++;
        count++;
      }
      break;
    }
    default: {}
    }
    len++;
  }
  return count == 4;
}

int main(int argc, char *argv[]) {
  // forward-state: argc == 1;
  //                len(argv[0]) < MAX_SIZE
  if (argc != 2) {
    cout << "Must have one argument: <parsed string>" << endl;
    return 0;
  }
  // state: argc == 1

  const char *arg = argv[1];
  if (strlen(arg) > MAX_SIZE) {
    // state: len(input) >= MAX_SIZE
    cerr << "The given string is too long..." << endl;
    return 0;
  }

  // state: len(input) < MAX_SIZE
  string input(argv[1]);
  cout << "Given string: " << input << endl;
  // forward-state: <<check_string>>
  if (check_string(input)) {
    // forward-state: <<trigger_fault>>
    // state: check_string(input) == true
    cout << "SUCCESS" << endl;
    trigger_fault();
    return 1;
  }
  return 0;
}

//
// Some non-relevant code to test the instrumentation
//

struct Foo {
  int bar;
  int baz;
};

Foo *get1(const Foo &test) { return const_cast<Foo *>(&test); }

Foo &get2(Foo &test, Foo &test2) { // does not work.
  if (&test == &test2)
    return test;
  else
    return test2;
}
