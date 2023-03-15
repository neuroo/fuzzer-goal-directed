#include <iostream>
#include <stdexcept>
#include <string>
using namespace std;

#define FOO 1

void dump(const char *str) { (void *)(str); }

void callMe() {
  if (FOO)
    throw new logic_error("woot");
  else if (!FOO)
    return;

  switch (FOO) {
  case 0:
    callMe();

  case 1:
    callMe();
    return;

  default:
    return;
  }

  if (FOO) {
    throw new logic_error("ww");
  }

  if (FOO)
    throw new logic_error("111");
  else if (FOO)
    throw new logic_error("211");
  else if (FOO)
    throw new logic_error("212");
  else if (FOO)
    throw new logic_error("213");
  else
    throw new logic_error("214");

  if (FOO)
    dump("iff");
  else if (FOO)
    dump("else if");

  if (FOO)
    dump("aff");
  else if (!FOO)
    dump("else if");
  else
    return;
}

int main(int argc, char *argv[]) {
  try {
    callMe();
  } catch (const exception &e) {
    cerr << "except: " << e.what() << endl;
  } catch (...) {
    throw new logic_error("ex");
  }
  return 0;
}
