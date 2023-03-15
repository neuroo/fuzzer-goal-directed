#include "evolution.h"
#include "common/logger.h"
#include "seeds.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <algorithm>
#include <fstream>
#include <iostream>
using namespace std;

namespace fuzz {

static const string SEED_STRING = "string";
static const string SEED_FILE = "file";

bool SeedHandler::parse(const string &seed_file) {
  fs::path fp(seed_file);
  if (!fs::is_regular_file(fp)) {
    return false;
  }

  ifstream file(seed_file.c_str(), ios::in);
  string line;
  bool errored = false;
  while (!file.eof()) {
    line.clear();
    errored = false;
    file >> line;
    ga::Individual ind = create(line, errored);
    if (!errored) {
      values.push_back(ind);
    }
  }
  file.close();

  LOG(INFO) << "Loaded " << values.size() << " seeds";
  for (auto &individual : values) {
    LOG(INFO) << individual.toString();
  }
  return true;
}

ga::Individual SeedHandler::create(const std::string &line, bool &errored) {
  size_t separator = line.find(",");
  if (separator == string::npos) {
    errored = true;
    return ga::Individual(memory);
  }
  string kind = line.substr(0, separator);
  if (kind == SEED_STRING) {
    string parsed_data = line.substr(separator + 1);
    uint32_t size = parsed_data.size();
    uint8_t *data =
        reinterpret_cast<uint8_t *>(const_cast<char *>(parsed_data.c_str()));

    ga::Individual i(memory);
    i.set(data, size);
    return i;
  } else if (kind == SEED_FILE) {
    string file_path = line.substr(separator + 1);
    fs::path fp(file_path);
    if (!fs::is_regular_file(fp)) {
      LOG(INFO) << "The file " << file_path << " doesn't exist";
      errored = true;
      return ga::Individual(memory);
    }
    ifstream seed_file(file_path.c_str(), ios::in | ios::binary);

    uint32_t size = 0;
    const uint32_t chunk_size = 8192;
    uint32_t allocated_size = chunk_size;
    uint8_t *data = new uint8_t[allocated_size]; // need to truncate/adjust the
                                                 // size of the buffer
                                                 // when loading data into it
    uint8_t *tmp = nullptr;

    while (seed_file) {
      char c;
      seed_file.get(c);
      if (seed_file) {
        size++;
        if (size > allocated_size) {
          allocated_size += chunk_size;
          tmp = new uint8_t[allocated_size];
          std::copy(data, data + size, tmp);
          delete[] data;
          data = tmp;
          tmp = nullptr;
        }
        data[size - 1] = static_cast<uint8_t>(c);
      }
    }

    ga::Individual i(memory);
    i.set(data, size);
    delete[] data;
    return i;
  } else {
    LOG(ERROR) << "Unhandled seed kind: " << kind;
    errored = true;
  }
  return ga::Individual(memory);
}
}
