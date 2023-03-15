extern void __coverage_reach_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_skip_block(const unsigned long, const unsigned int, const unsigned int);
extern void __coverage_enter_func(const unsigned long);
extern void __coverage_exit_func(const unsigned long);
extern void __coverage_kill(const unsigned long);
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
  std::vector<std::string> findMatchingAddresses(Func func) {std::vector<std::string> __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(99);
    std::vector<std::string> results;
    for (auto itr = _addresses.begin(), end = _addresses.end(); itr != end;
         ++itr) { __coverage_reach_block(99, __coverage_pred_block, 4); __coverage_pred_block = 4; 
      // call the function passed into findMatchingAddresses and see if it
      // matches
      if (func(*itr)) { __coverage_reach_block(99, __coverage_pred_block, 3); __coverage_pred_block = 3; 
        results.push_back(*itr);
      }
    }
    __coverage_ret_value = (results); __coverage_exit_func(99); return __coverage_ret_value;
  }

private:
  std::vector<std::string> _addresses;
};

AddressBook global_address_book;

vector<string> findAddressesFromOrgs() {vector<string> __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(108);
  __coverage_ret_value = (global_address_book.findMatchingAddresses(
      // we're declaring a lambda here; the [] signals the start
      [](const string &addr) { return addr.find(".org") != string::npos; }))unsigned int __coverage_pred_block = 0; __coverage_enter_func(112);; __coverage_exit_func(108); return __coverage_ret_value;
}

// general C++ tests, not specific to C++11
void play_general() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(116);

  {

    struct A {
      int x = 7;
      virtual ~A() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(151); __coverage_exit_func(151); }
      virtual void foo() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(154); cout << x << endl;  __coverage_exit_func(154); }

      A &operator=(const A &a) {unsigned int __coverage_pred_block = 0; __coverage_enter_func(158);
        x = a.x;
        struct A & __coverage_ret_value_1 = (*this); __coverage_exit_func(158); return __coverage_ret_value_1;
      }
    };

    struct B : public A {
      int y[4] = {1, 2, 3, 4};

      virtual void foo() {unsigned int __coverage_pred_block = 0; __coverage_enter_func(162); cout << y[1] << endl;  __coverage_exit_func(162); }
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

    auto funcTakingAs = [](A as[3]) unsigned int __coverage_pred_block = 0; __coverage_enter_func(130);{
      // likely crash here when called with B[]
      bool allAre7 = true;
      for (int i = 0; i < 3; ++i) { __coverage_reach_block(130, __coverage_pred_block, 5); __coverage_pred_block = 5; 
        if (as[i].x != 7)
          { __coverage_reach_block(130, __coverage_pred_block, 4); __coverage_pred_block = 4; allAre7 = false;}
        cout << "xxx " << as[i].x << '\n';
      }
      return allAre7;
    };
    bool allAre7 = funcTakingAs(bs);

    // This here surprising compiles also, same array/pointer decay reason:
    auto funcTaking4Bs = [](B bs[4]) unsigned int __coverage_pred_block = 0; __coverage_enter_func(140);{ __coverage_exit_func(140); };
    funcTaking4Bs(bs);

    int i = 1;
    auto f = [&i](int k) unsigned int __coverage_pred_block = 0; __coverage_enter_func(143);{
      // LMBGEN: store {{.*}} @[[LFC]], i64 0, i64 1
      // LMBUSE: br {{.*}} !prof ![[LF1:[0-9]+]]
      if (i < 1) { __coverage_reach_block(143, __coverage_pred_block, 4); __coverage_pred_block = 4; 
        return false;
      }
      // LMBGEN: store {{.*}} @[[LFC]], i64 0, i64 2
      // LMBUSE: br {{.*}} !prof ![[LF2:[0-9]+]]
      return k && i;
    };

    for (i = 0; i < 10; ++i)
      { __coverage_reach_block(116, __coverage_pred_block, 2); __coverage_pred_block = 2; f(9 - i);}
  }
 __coverage_exit_func(116); }

int main() {int __coverage_ret_value;unsigned int __coverage_pred_block = 0; __coverage_enter_func(166); __coverage_exit_func(166); }
