
#include "utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace std;

#include "murmur3.h"

namespace utils {

uint32_t Rand::next_number(const uint32_t limit) {
  if (limit < 1)
    return 0;
  return distributor(generator) % limit;
}

uint32_t Rand::next_int() { return distributor(generator); }

uint8_t Rand::next_char() { return static_cast<uint8_t>(next_number(256)); }

char Rand::next_ascii() {
  if (next_bool())
    return next_char();
  const char *llvm_fuzzer_specials = "!*'();:@&=+$,/?%#[]123ABCxyz-`~.";
  return llvm_fuzzer_specials[next_number(sizeof(llvm_fuzzer_specials) - 1)];
}

bool Rand::next_bool() { return next_number(2) == 0; }

string hex_dump(uint8_t *data, uint32_t size) {
  if (size < 1)
    return "";

  const char *const start =
      reinterpret_cast<const char *>(const_cast<const uint8_t *>(data));
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

string shell_escape(const string &input) {
  const bool has_double_quote = input.find('"') != string::npos;
  ostringstream oss;
  if (has_double_quote)
    oss << '"';

  for (auto &c : input) {
    if (c < 32) {
      oss << "\\x" << uppercase << setfill('0') << setw(2) << std::hex
          << static_cast<uint8_t>(c);
    } else if (c == '"' || c == '\\' || c == '\'') {
      oss << '\\' << c;
    } else {
      oss << c;
    }
  }
  if (has_double_quote)
    oss << '"';
  return oss.str();
}

string html_escape(const uint8_t c) {
  switch (c) {
  case '&':
    return "&amp;";
  case '<':
    return "&lt;";
  case '>':
    return "&gt;";
  case '"':
    return "&quot;";
  case '\'':
    return "&apos;";
  default:
    if (c < 32 || c > 127 || c == '\\' || c == '"') {
      ostringstream oss;
      oss << "&#" << (unsigned int)c << ";";
      return oss.str();
    } else {
      return string(1, c);
    }
  }
}

container::uint128_t hash(uint8_t *data, uint32_t size) {
  uint64_t result[2];
  MurmurHash3_x64_128(data, size, 0, result);
  return container::uint128_t(result[0], result[1]);
}

void replace_all(string &str, const string &from, const string &to) {
  if (from.empty())
    return;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}
}
