#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
using namespace std;

#include "shared-data/shared-data.h"
using namespace shm;

TraceElement create() {
  return TraceElement(E_TRUE_BRANCH, /*thread_id*/ 0x4141, rand() % 40,
                      rand() % 40, rand() % 40);
}

int testcase_id() { return rand() % 256000 + 1; }

int main(int argc, char *argv[]) {
  using namespace std;

  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " (runtime|fuzzer)" << endl;
    return 1;
  }

  const std::string which = argv[1];

  uint64_t progress = 0;
  if (which == "runtime") {
    cout << "Start runtime..." << endl;
    SHMRuntimeWriter writer;
    while (true) {
      const int t_id = testcase_id();
      writer.container->set_status(t_id, E_STATUS_ACTIVE);
      if (rand() % 10) {
        writer.container->set_status(t_id, E_STATUS_TERMINATED);
      }
      std::cout << progress << "\r";
      std::cout.flush();
      ++progress;
    }
  } else {
    cout << "Start creator/receiver..." << endl;
    SHMFuzzerHandler reader(true);
    while (true) {
      reader.container->get_status(testcase_id());
      if (0 == (rand() % 10)) {
        reader.container->clear_statuses();
      }

      std::cout << progress << "\r";
      std::cout.flush();
      ++progress;
    }
  }

  return 0;
}
