#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
using namespace std;

auto foo = [] { std::cout << "whhot"; };

class AddressBook {
public:
  // using a template allows us to ignore the differences between functors,
  // function pointers
  // and lambda
  template <typename Func>
  std::vector<std::string> findMatchingAddresses(Func func) {
    std::vector<std::string> results;
    for (auto itr = _addresses.begin(), end = _addresses.end(); itr != end;
         ++itr) {
      // call the function passed into findMatchingAddresses and see if it
      // matches
      if (func(*itr)) {
        results.push_back(*itr);
      }
    }
    return results;
  }

private:
  std::vector<std::string> _addresses;
};

AddressBook global_address_book;

vector<string> findAddressesFromOrgs() {
  return global_address_book.findMatchingAddresses(
      // we're declaring a lambda here; the [] signals the start
      [](const string &addr) { return addr.find(".org") != string::npos; });
}

// general C++ tests, not specific to C++11
void play_general() {

  {

    struct A {
      int x = 7;
      virtual ~A() {}
      virtual void foo() { cout << x << endl; }

      A &operator=(const A &a) {
        x = a.x;
        return *this;
      }
    };

    struct B : public A {
      int y[4] = {1, 2, 3, 4};

      virtual void foo() { cout << y[1] << endl; }
    };

    static_assert(sizeof(A) != sizeof(B), ""); // not guaranteed

    // Now this here is a terrible example of type-safety gone wrong:
    // The B[3] can be passed to a func taking type A[3], effectively
    // silenty reinterpret-casting memory locations to B*.
    // I knew C++ (due to its C legacy) lets T[] decay to a T* (which is pretty
    // bad alrdy), but I didn't think this allows decaying B[3] to A[3].
    // See
    // http://stackoverflow.com/questions/12064333/assigning-derived-class-array-to-base-class-pointer
    // This problem can be avoided by using std::array<A, 3> btw.

    B bs[3]; // = {}

    auto funcTakingAs = [](A as[3]) {
      // likely crash here when called with B[]
      bool allAre7 = true;
      for (int i = 0; i < 3; ++i) {
        if (as[i].x != 7)
          allAre7 = false;
        cout << "xxx " << as[i].x << '\n';
      }
      return allAre7;
    };
    bool allAre7 = funcTakingAs(bs);

    // This here surprising compiles also, same array/pointer decay reason:
    auto funcTaking4Bs = [](B bs[4]) {};
    funcTaking4Bs(bs);

    int i = 1;
    auto f = [&i](int k) {
      // LMBGEN: store {{.*}} @[[LFC]], i64 0, i64 1
      // LMBUSE: br {{.*}} !prof ![[LF1:[0-9]+]]
      if (i < 1) {
        return false;
      }
      // LMBGEN: store {{.*}} @[[LFC]], i64 0, i64 2
      // LMBUSE: br {{.*}} !prof ![[LF2:[0-9]+]]
      return k && i;
    };

    for (i = 0; i < 10; ++i)
      f(9 - i);
  }
}

int main() {}
