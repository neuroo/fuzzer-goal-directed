#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/asio.hpp>
#include <boost/atomic.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

//
// /!\ Due to the use of restbed, all the code here is AGLP /!\
//

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
using namespace std;

#include <restbed>
using namespace restbed;

#include "shared-data/shared-data.h"
using namespace shm;

#define DEFAULT_PORT 8889
static const string ERROR_JSON_MESSAGE = "{\"error\": true, \"data\":null}";

// Returns a properly quoted JSON string
string json_escape(const string &what) {
  ostringstream oss;
  oss << "\"";
  for (auto &c : what) {
    switch (c) {
    case '\r':
      oss << "\\u000d";
      break;
    case '\n':
      oss << "\\u000a";
      break;
    case '"':
      oss << "\\\"";
      break;
    default:
      oss << c;
    }
  }
  oss << "\"";
  return oss.str();
}

string custom_error_message(const std::string &what) {
  ostringstream oss;
  oss << "{\"error\": true, \"data\": " << json_escape(what) << "}";
  return oss.str();
}

uint64_t to_unique_block_id(const uint32_t func_id, const uint32_t block_id) {
  return (((uint64_t)func_id) << 32) + (uint64_t)block_id;
}

string to_json(const Container::trace_t &trace) {
  ostringstream oss;
  oss << "{\"error\": false, \"data\":[";
  bool is_first = true;
  for (auto &trace_elmt : trace) {
    if (!is_first) {
      oss << ",";
    } else {
      is_first = false;
    }
    oss << "{\"kind\":\"" << traceKindName(trace_elmt.kind) << "\"";
    if (trace_elmt.func_id > 0) {
      oss << ","
          << "\"thread\":" << trace_elmt.thread_id << ","
          << "\"func\":" << trace_elmt.func_id;
      if (trace_elmt.cur_block_id > 0) {
        oss << ","
            << "\"pred\":"
            << to_unique_block_id(trace_elmt.func_id, trace_elmt.pred_block_id)
            << ",\"cur\":"
            << to_unique_block_id(trace_elmt.func_id, trace_elmt.cur_block_id);
      }
    }
    oss << "}";
  }
  oss << "]}";
  return oss.str();
}

void get_trace_handler(const shared_ptr<Session> session) {
  const auto request = session->get_request();

  uint64_t testcase_id = 0;
  Container::trace_t *trace = nullptr;
  SHMRuntimeWriter handler;

  if (request->has_query_parameter("tid")) {
    request->get_query_parameter("tid", testcase_id);
    if (testcase_id > 0) {
      while (trace == nullptr) {
        trace = handler.container->get_trace(testcase_id);
      }
    }
  }

  if (!trace) {
    string error_message = custom_error_message(
        "Cannot find the trace for tid: " + std::to_string(testcase_id));
    session->close(NOT_FOUND, error_message,
                   {{"Content-Length", std::to_string(error_message.length())},
                    {"Content-Type", "application/json"},
                    {"Connection", "close"}});
  } else {
    try {
      string json_str = to_json(*trace);
      session->close(OK, json_str,
                     {{"Content-Length", std::to_string(json_str.length())},
                      {"Content-Type", "application/json"},
                      {"Connection", "close"}});
    } catch (exception &e) {
      string error_message = custom_error_message(e.what());
      session->close(
          INTERNAL_SERVER_ERROR, error_message,
          {{"Content-Length", std::to_string(error_message.length())},
           {"Content-Type", "application/json"},
           {"Connection", "close"}});
    }
  }
}

void get_container_handler(const shared_ptr<Session> session) {
  const auto request = session->get_request();
  SHMRuntimeWriter handler;

  try {
    string message = handler.container->toString();
    session->close(OK, message,
                   {{"Content-Length", std::to_string(message.length())},
                    {"Content-Type", "text/plain"},
                    {"Connection", "close"}});
  } catch (exception &e) {
    string error_message = custom_error_message(e.what());
    session->close(INTERNAL_SERVER_ERROR, error_message,
                   {{"Content-Length", std::to_string(error_message.length())},
                    {"Content-Type", "text/plain"},
                    {"Connection", "close"}});
  }
}

void install_service(unsigned int port) {
  auto trace_resource = std::make_shared<Resource>();
  trace_resource->set_path("/trace");
  trace_resource->set_method_handler("GET", get_trace_handler);

  auto container_resource = std::make_shared<Resource>();
  container_resource->set_path("/container");
  container_resource->set_method_handler("GET", get_container_handler);

  auto settings = make_shared<Settings>();
  settings->set_port(port);
  settings->set_default_header("Connection", "close");

  Service service;
  service.publish(trace_resource);
  service.publish(container_resource);

  cout << "Runtime trace service started on http://localhost:" << port
       << "/trace/?tid=XXX" << endl;
  service.start(settings);
}

int main(int argc, char *argv[]) {
  try {
    po::options_description all_opts(
        "Service to convert runtime traces to simpler format");

    // clang-format off
    po::options_description general_options("General options");
    general_options.add_options()
      ("help,h", "produce this help message")
      ("port,p", po::value<unsigned int>()->default_value(DEFAULT_PORT), "port to be used for the HTTP service");
    // clang-format on

    all_opts.add(general_options);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, all_opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << all_opts << endl;
      return 0;
    }
    install_service(vm["port"].as<unsigned int>());
    return 0;
  } catch (exception &e) {
    cerr << "Exception: " << e.what() << endl;
    return 1;
  }
}
