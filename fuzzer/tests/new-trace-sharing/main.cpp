#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unistd.h>
using namespace std;

#include "trace-element.h"

#include "client.h"
#include "server.h"
namespace comm = fuzz::shared;

//
// Utilities
//
string hex_dump(const char *data, size_t size) {
  if (size < 1)
    return "";

  const char *const start = data;
  const char *const end = start + size;
  const char *line = start;
  const size_t aWidth = 16;

  ostringstream oss;

  while (line != end) {
    oss.width(4);
    oss.fill('0');
    oss << std::hex << line - start << " : ";
    std::size_t lineLength =
        std::min(aWidth, static_cast<std::size_t>(end - line));
    for (std::size_t pass = 1; pass <= 2; ++pass) {
      for (const char *next = line; next != end && next != line + aWidth;
           ++next) {
        char ch = *next;
        switch (pass) {
        case 1:
          oss << (ch < 32 ? '.' : ch);
          break;
        case 2:
          if (next != line)
            oss << " ";
          oss.width(2);
          oss.fill('0');
          oss << std::hex << std::uppercase
              << static_cast<int>(static_cast<unsigned char>(ch));
          break;
        }
      }
      if (pass == 1 && lineLength != aWidth)
        oss << std::string(aWidth - lineLength, ' ');
      oss << " ";
    }
    oss << std::endl;
    line = line + lineLength;
  }
  return oss.str();
}

bool deserializeFromArchive(const std::string &input,
                            SendableTrace &sendable_trace) {
  try {
    std::istringstream iss(input);
    cereal::BinaryInputArchive iarchive(iss);
    iarchive(sendable_trace);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "deserializeFromArchive- " << e.what() << std::endl
              << hex_dump(input.c_str(), input.length()) << std::endl;
    return false;
  }
}

bool serializeToArchive(const SendableTrace &sendable_trace, std::string &str) {
  try {
    std::ostringstream oss;
    cereal::BinaryOutputArchive oarchive(oss);
    oarchive(sendable_trace);
    str = oss.str();
    return true;
  } catch (const std::exception &e) {
    std::cerr << "serializeFromArchive- " << e.what() << std::endl;
    return false;
  }
}

SendableTraceElement create_element() {
  return SendableTraceElement(E_TRUE_BRANCH, /*thread_id*/ 0x4141,
                              rand() % 40 + 1, rand() % 40 + 1,
                              rand() % 40 + 1);
}

SendatableTraceContainer create(uint32_t length) {
  SendatableTraceContainer trace;
  trace.set_id(rand() + 1);
  for (uint32_t i = 0; i < length; i++) {
    trace.add(create_element());
  }
  return trace;
}

long lrand() { return rand() % 65526 + 1; }

static const std::string endpoint = "/tmp/endpoint_foo";

void setup_receiver() {
  //
  comm::server_t server(endpoint);
}

void setup_sender(const std::string &testcase_id) {
  comm::client_t client(endpoint);
  //
}

int main(int argc, char *argv[]) {
  using namespace std;
  std::srand(time(nullptr) + 1);

  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " (runtime|fuzzer)" << endl;
    return 1;
  }

  const std::string which = argv[1];
  if (which == "fuzzer") {
    setup_receiver();
  } else {
    std::string testcase_id = "1";
    if (argc > 2)
      testcase_id = argv[2];
    setup_sender(testcase_id);
  }
}
