#include "ui.h"
#include "common/logger.h"
#include "crash-analyzer.h"
#include "knowledge.h"
#include "utils.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/graph/graphviz.hpp>
namespace bgl = boost;

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

#include "leveldb/cache.h"
#include "leveldb/filter_policy.h"

#include "json/json.h"
#include "json/reader.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
using namespace std;

namespace fuzz {

namespace ui {

static const string UI_LEVELDB_FILENAME = "ui.db";

//
// JSON utils
//
static Json::Value create_initial_message(const string &message_kind,
                                          bool error = false,
                                          bool is_array = false) {
  Json::Value msg;
  msg["error"] = error;
  msg["kind"] = message_kind;
  msg["data"] = Json::Value(is_array ? Json::arrayValue : Json::objectValue);
  return msg;
}

static Json::Value create_null_message(const string &message_kind) {
  Json::Value msg;
  msg["error"] = true;
  msg["kind"] = message_kind;
  msg["data"] = Json::Value(Json::nullValue);
  return msg;
}

static Json::Value create_array() { return Json::Value(Json::arrayValue); }

static string to_json_string(const Json::Value &json) {
  return (Json::FastWriter()).write(json);
}

static uint32_t get_json_id(const Json::Value &data) {
  if (data.isNull() || !data.isUInt()) {
    LOG(ERROR) << "Malformed data? Cannot read integer from: "
               << to_json_string(data);
    return 0;
  }
  return data.asUInt();
}

static string get_json_string(const Json::Value &data) {
  if (data.isNull() || !data.isString()) {
    LOG(ERROR) << "Malformed data? Cannot read string from: "
               << to_json_string(data);
    return 0;
  }
  return data.asString();
}

static Json::Value from_score(const score_t &score) {
  Json::Value json;
  json["absolute"] = score.absolute;
  json["diff"] = score.diff;
  json["norm"] = score.norm();
  return json;
}

static Json::Value from_measure(const measure_t &measure) {
  Json::Value json;
  json["goal"] = from_score(measure.goal);
  json["edge"] = from_score(measure.edge);
  json["length"] = (uint32_t)measure.length;
  return json;
}

//
// UICrashReader
//
UICrashReader::UICrashReader(std::unique_ptr<UIDataStore> &data_store,
                             const string &idir_str)
    : data_store(data_store) {
  initialize(idir_str);
}

void UICrashReader::initialize(const string &idir_str) {
  idir = fs::path(idir_str);
  crashes = idir / "crashes";
  results = idir / "results";
}

void UICrashReader::run() {
  if (!fs::is_directory(crashes)) {
    LOG(INFO) << "Crash data not ready...";
    return;
  }

  for (auto &x : fs::directory_iterator(crashes)) {
    const fs::path &current_path = x.path();
    if (!fs::is_directory(current_path))
      continue;
    const string crash_id = current_path.filename().string();
    read_crash_dir(crash_id, current_path);
  }
}

void UICrashReader::read_crash_dir(const string &crash_id,
                                   const fs::path &crash_dir) {
  if (locked_crash_ids.find(crash_id) != locked_crash_ids.end())
    return;

  std::lock_guard<std::mutex> lock(crashes_mutex);
  auto crash_iter = crashes_mapping.find(crash_id);
  if (crash_iter == crashes_mapping.end()) {
    crashes_mapping.insert({crash_id, std::set<uint64_t>()});
    crash_iter = crashes_mapping.find(crash_id);
  }

  for (auto &x : fs::directory_iterator(crash_dir)) {
    const fs::path &current_path = x.path();
    if (!fs::is_directory(current_path))
      continue;
    const string testcase_id_str = current_path.filename().string();
    try {
      const uint64_t testcase_id =
          boost::lexical_cast<uint64_t>(testcase_id_str);
      if (testcase_id < 1)
        continue;
      if (associated_bimap(testcase_id, crash_id)) {
        continue;
      }

      crash_iter->second.insert(testcase_id);
      if (crash_iter->second.size() >= MAX_NUMBER_CRASH_PER_KIND) {
        locked_crash_ids.insert(crash_id);
        LOG(INFO) << "Full crash_id: " << crash_id;
      }
    } catch (exception &e) {
      LOG(ERROR) << "Exception- " << e.what();
    }
  }
}

bool UICrashReader::associated_bimap(const uint64_t testcase_id,
                                     const std::string &crash_id) {
  std::lock_guard<std::mutex> lock(bimap_mutex);

  auto bi_iter = bimap.find(testcase_id);
  if (bi_iter != bimap.end())
    return true;

  size_t index = 0;
  auto iter = std::find(crash_ids.begin(), crash_ids.end(), crash_id);
  if (iter == crash_ids.end()) {
    index = crash_ids.size();
    crash_ids.push_back(crash_id);
  } else {
    index = std::distance(crash_ids.begin(), iter);
  }
  bimap.insert({testcase_id, index});
  return false;
}

string UICrashReader::get_crash_id(const uint64_t testcase_id) {
  std::lock_guard<std::mutex> lock(bimap_mutex);
  auto iter = bimap.find(testcase_id);
  if (iter == bimap.end())
    return string();
  if (iter->second >= crash_ids.size())
    return string();
  return crash_ids[iter->second];
}

Json::Value UICrashReader::get(const uint64_t testcase_id) {
  string crash_id = get_crash_id(testcase_id);
  if (crash_id.empty()) {
    LOG(ERROR) << "Cannot find crash_id for testcase " << testcase_id;
    return create_null_message("crash");
  }

  // Construct the path to the "info.json" file
  fs::path info_json =
      crashes / crash_id / std::to_string(testcase_id) / "info.json";
  string contents;
  if (!fs::is_regular_file(info_json)) {
    LOG(ERROR) << "Cannot find crash file for testcase " << testcase_id
               << " in " << info_json.string();
    return create_null_message("crash");
  } else {
    fs::ifstream ifs(info_json);
    ostringstream oss;
    oss << ifs.rdbuf();
    contents = oss.str();
  }

  Json::Value info;
  Json::Reader reader;
  if (contents.empty() || !reader.parse(contents, info)) {
    LOG(ERROR) << "Cannot parse crash file " << info_json.string();
    LOG(ERROR) << "Contents: " << contents;
    return create_null_message("crash");
  }

  // Fetch the actual testcase from the data_store;
  bool fetch_error = false;
  string buffer = data_store->get(testcase_id, fetch_error);
  if (fetch_error) {
    LOG(ERROR) << "Cannot fetch testcase info for testcase_id=" << testcase_id;
    return create_null_message("crash");
  }

  Json::Value json = create_initial_message("crash");
  json["data"]["crash_id"] = crash_id;
  json["data"]["testcase_id"] = testcase_id;
  json["data"]["info"] = info;
  json["data"]["testcase"] = Json::Value();
  json["data"]["testcase"]["length"] = static_cast<uint32_t>(buffer.length());
  json["data"]["testcase"]["buffer"] = "";

  // Create a nicer representation for the data...
  uint8_t *raw_buffer =
      reinterpret_cast<uint8_t *>(const_cast<char *>(buffer.c_str()));
  string repr = utils::hex_dump(raw_buffer, buffer.length());

  vector<string> repr_lines;
  boost::split(repr_lines, repr, boost::is_any_of("\n\r"),
               boost::token_compress_on);

  json["data"]["testcase"]["repr"] = Json::Value(Json::arrayValue);
  for (auto &x : repr_lines) {
    json["data"]["testcase"]["repr"].append(x);
  }
  return json;
}

std::set<uint64_t> UICrashReader::get_crashers() {
  std::set<uint64_t> crashers;
  std::lock_guard<std::mutex> lock(bimap_mutex);

  std::transform(
      bimap.begin(), bimap.end(), std::inserter(crashers, crashers.begin()),
      [](const map<uint64_t, size_t>::value_type &pair) { return pair.first; });
  return crashers;
}

std::set<std::string> UICrashReader::get_crash_ids() {
  std::set<std::string> set_crash_ids;
  std::lock_guard<std::mutex> lock(bimap_mutex);

  std::copy(crash_ids.begin(), crash_ids.end(),
            std::inserter(set_crash_ids, set_crash_ids.begin()));
  return set_crash_ids;
}

map<string, set<uint64_t>> UICrashReader::get_crash_mapping() {
  std::lock_guard<std::mutex> lock(crashes_mutex);
  map<string, set<uint64_t>> new_map(crashes_mapping);
  return new_map;
}

//
// UIDataStore
//
__attribute__((no_sanitize_address))
UIDataStore::UIDataStore(const string &store_path_str) {
  initialize(store_path_str);
}

__attribute__((no_sanitize_address)) void
UIDataStore::initialize(const string &store_path_str) {
  store_path = fs::path(store_path_str);
  store_path = store_path / UI_LEVELDB_FILENAME;
  if (fs::exists(store_path)) {
    fs::remove_all(store_path);
    fs::create_directories(store_path);
  }

  try {
    options.create_if_missing = true;
    options.error_if_exists = false;
    options.block_cache = leveldb::NewLRUCache(100 * 1048576); // 100MB cache
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    leveldb::Status status =
        leveldb::DB::Open(options, store_path.string(), &db);
    if (!status.ok()) {
      LOG(ERROR) << "Cannot create the leveldb database";
      delete db;
      db = nullptr;
      errored = true;
      return;
    }
  } catch (exception &e) {
    LOG(ERROR) << "Exception- " << e.what();
    errored = true;
    db = nullptr;
  }
  LOG(INFO) << "initialized DB";
}

__attribute__((no_sanitize_address)) bool
UIDataStore::set(const uint64_t tescase_id,
                 const ga::ShareableIndividual &ind) {
  string key = to_key(tescase_id);
  string value = to_repr(ind);
  leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
  return s.ok();
}

__attribute__((no_sanitize_address)) string
UIDataStore::get(const uint64_t tescase_id, bool &fetch_error) const {
  string value;
  string key = to_key(tescase_id);

  leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
  if (s.ok()) {
    fetch_error = false;
    return from_rep(value, fetch_error);
  }
  fetch_error = true;
  return "";
}

__attribute__((no_sanitize_address)) string
UIDataStore::to_key(const uint64_t testcase_id) const {
  return std::to_string(testcase_id);
}

__attribute__((no_sanitize_address)) string
UIDataStore::to_repr(const ga::ShareableIndividual &ind) const {
  string value;
  if (ind.length > 0) {
    value.assign(ind.buffer, ind.buffer + ind.length);
  }

  Json::Value json;
  json["id"] = ind.id;
  json["length"] = static_cast<uint32_t>(ind.length);
  json["value"] = value;

  return (Json::FastWriter()).write(json);
}

__attribute__((no_sanitize_address)) string
UIDataStore::from_rep(const string &rep, bool &fetch_error) const {
  Json::Value json;
  Json::Reader reader;
  if (reader.parse(rep, json)) {
    return json["value"].asString();
  } else {
    fetch_error = true;
    return "";
  }
}

__attribute__((no_sanitize_address)) UIDataStore::~UIDataStore() {
  if (options.block_cache) {
    delete options.block_cache;
  }
  if (options.filter_policy) {
    delete options.filter_policy;
  }
  if (db) {
    delete db;
  }
}

//
// UIStoreGraph
//
UIStoreGraph::UIStoreGraph(const instr::Store *store) { initialize(store); }

// Need to iterate over our entire store build a graph
void UIStoreGraph::initialize(const instr::Store *store) {
  using namespace instr;
  if (store == nullptr)
    return;

  StoreImpl &s_impl = const_cast<Store *>(store)->store();
  for (const auto &m_elmt : s_impl.elements) {
    switch (m_elmt.second->getKind()) {
    case Element::E_FUNCTION: {
      std::shared_ptr<FunctionElement> func_elmt =
          std::static_pointer_cast<FunctionElement>(m_elmt.second);
      if (!func_elmt)
        continue;
      // That's the root
      handle_function(s_impl, func_elmt);
      break;
    }
    }
  }
}

// XXX for now, the min_coverage is never updated...
void UIStoreGraph::update_coverage(const coverage_t &local_coverage) {
  for (auto &m_elmt : local_coverage) {
    auto iter = coverage.find(m_elmt.first);
    if (iter == coverage.end()) {
      coverage.insert({m_elmt.first, m_elmt.second});
      if (m_elmt.second > max_coverage)
        max_coverage = m_elmt.second;
    } else {
      iter->second += m_elmt.second;
      if (iter->second > max_coverage)
        max_coverage = iter->second;
    }
  }
}

void UIStoreGraph::handle_function(
    const instr::StoreImpl &s_impl,
    const std::shared_ptr<instr::FunctionElement> &func_elmt) {
  using namespace instr;

  const element_id id = func_elmt->getId();
  // Already processed function
  if (vertices.find(id) != vertices.end())
    return;

  coverage.insert({id, 0});

  vertex_t v = bgl::add_vertex(graph);
  graph[v].element = id;
  vertices.insert({id, v});

  // Create and store the representation of this function...
  std::shared_ptr<ElementFunctionMessage> elmt_func_message_ptr =
      std::make_shared<ElementFunctionMessage>();
  elmt_func_message_ptr->id = id;
  elmt_func_message_ptr->name = func_elmt->name;

  // Process basic blocks in the functions
  for (const auto block_id : func_elmt->blocks) {
    elmt_func_message_ptr->blocks.push_back(block_id);

    auto e = s_impl.elements.find(block_id)->second;
    if (e->getKind() != Element::E_BLOCK)
      continue;
    auto block_elmt = std::static_pointer_cast<BlockElement>(e);
    if (!block_elmt)
      continue;
    handle_block(s_impl, v, block_elmt);
  }

  messages.insert({id, elmt_func_message_ptr});
}

void UIStoreGraph::handle_block(
    const instr::StoreImpl &s_impl, const vertex_t &func_vertex,
    const std::shared_ptr<instr::BlockElement> &block_elmt) {
  using namespace instr;

  const element_id id = block_elmt->getId();
  // Already processed function
  if (vertices.find(id) != vertices.end())
    return;

  coverage.insert({id, 0});

  vertex_t v = bgl::add_vertex(graph);
  graph[v].element = id;
  vertices.insert({id, v});

  // Add vertex from `func_vertex` to `v`
  auto edge = bgl::edge(func_vertex, v, graph);
  bgl::add_edge(func_vertex, v, graph);

  // Create and store the representation of this block...
  std::shared_ptr<ElementBlockMessage> elmt_block_message_ptr =
      std::make_shared<ElementBlockMessage>();
  elmt_block_message_ptr->id = id;
  elmt_block_message_ptr->block_id = block_elmt->internal_block_id;

  // Process basic blocks in the functions
  for (const auto goal_id : block_elmt->summaries) {
    elmt_block_message_ptr->goals.push_back(goal_id);

    auto e = s_impl.elements.find(goal_id)->second;
    if (e->getKind() != Element::E_SUMMARY)
      continue;
    auto summary_elmt = std::static_pointer_cast<SummaryElement>(e);
    if (!summary_elmt)
      continue;
    handle_goal(s_impl, v, summary_elmt);
  }

  messages.insert({id, elmt_block_message_ptr});
}

void UIStoreGraph::handle_goal(
    const instr::StoreImpl &s_impl, const vertex_t &block_vertex,
    const std::shared_ptr<instr::SummaryElement> &summary_elmt) {
  using namespace instr;

  const element_id id = summary_elmt->getId();
  // Already processed function
  if (vertices.find(id) != vertices.end())
    return;

  vertex_t v = bgl::add_vertex(graph);
  graph[v].element = id;
  vertices.insert({id, v});

  // Add vertex from `block_vertex` to `v`
  auto edge = bgl::edge(block_vertex, v, graph);
  bgl::add_edge(block_vertex, v, graph);

  // Create and store the representation of this summary...
  std::shared_ptr<ElementGoalMessage> elmt_goal_message_ptr =
      std::make_shared<ElementGoalMessage>();
  elmt_goal_message_ptr->id = id;
  elmt_goal_message_ptr->type = typeName(summary_elmt->type_kind);
  elmt_goal_message_ptr->op = operatorName(summary_elmt->op);

  messages.insert({id, elmt_goal_message_ptr});
  // Terminal
}

void UIStoreGraph::to_dot() {
  ostringstream oss;
  bgl::write_graphviz(oss, graph, bgl::make_label_writer(bgl::get(
                                      &vertex_bundle_t::element, graph)));
  LOG(INFO) << "UIStoreGraph :=\n" << oss.str();
}

UIStoreGraph::~UIStoreGraph() {}

//
// UICrashSummarization
//
UICrashSummarization::UICrashSummarization(
    std::unique_ptr<UIDataStore> &data_store,
    std::unique_ptr<UIStoreGraph> &store_graph)
    : data_store(data_store), store_graph(store_graph) {

  vertex_t start_vertex = bgl::add_vertex(automaton);
  automaton[start_vertex].start = true;
  automaton[start_vertex].end = false;
  start = start_vertex;

  vertex_t end_vertex = bgl::add_vertex(automaton);
  automaton[end_vertex].end = true;
  automaton[end_vertex].start = false;
  end = end_vertex;
}

void UICrashSummarization::update_crashers(
    const std::set<uint64_t> &crashers_id) {

  if (!delay_processing.empty()) {
    set<uint64_t> processed;
    for (const auto &testcase_id : delay_processing) {
      bool fetch_error = false;
      string buffer = data_store->get(testcase_id, fetch_error);
      if (fetch_error) {
        LOG(INFO) << "Fetching error... Delaying integration of this testcase";
        continue;
      }
      processed.insert(testcase_id);
      insert_crasher(testcase_id, buffer);
    }

    for (const auto testcase_id : processed) {
      delay_processing.erase(testcase_id);
    }
  }

  for (const auto &testcase_id : crashers_id) {
    bool fetch_error = false;
    string buffer = data_store->get(testcase_id, fetch_error);
    if (fetch_error) {
      LOG(INFO) << "Fetching error... Delaying integration of this testcase";
      delay_processing.insert(testcase_id);
      continue;
    }
    insert_crasher(testcase_id, buffer);
  }
}

void UICrashSummarization::insert_crasher(const uint64_t testcase_id,
                                          string &buffer) {
  const size_t length = buffer.length();
  LOG(INFO) << "Insert crasher, tc_id=" << testcase_id << " length=" << length;

  vertex_t parent = start;
  for (size_t index = 0; index < length; index++) {
    const uint8_t c = buffer[index];

    // Do we have the vertex for this index?
    vertex_t current;
    auto vertex_iter = vertices.find(index);
    if (vertex_iter == vertices.end()) {
      // Insert the new vertex
      current = bgl::add_vertex(automaton);
      automaton[current].index = index;
      vertices.insert({index, current});
    } else {
      current = vertex_iter->second;
    }

    auto lookup_pair = std::make_pair(current, c);
    auto edge_iter = edges.find(lookup_pair);
    if (edge_iter == edges.end()) {
      auto edge = bgl::add_edge(parent, current, automaton);
      if (parent == start) {
        automaton[edge.first].epsilon = true;
      }
      automaton[edge.first].transition = c;
      automaton[edge.first].occurences = 1;
      edges.insert(std::make_pair(lookup_pair, edge.first));
    } else {
      automaton[edge_iter->second].occurences += 1;
    }
    parent = current;
  }

  // Add an edge from parent to the end
  auto edge = bgl::edge(parent, end, automaton);
  if (!edge.second) {
    auto e = bgl::add_edge(parent, end, automaton);
    automaton[e.first].epsilon = true;
  }
}

void UICrashSummarization::to_dot() {
  ostringstream oss;
  bgl::write_graphviz(
      oss, automaton,
      bgl::make_label_writer(bgl::get(&vertex_bundle_t::index, automaton)),
      make_edge_writer(bgl::get(&edge_bundle_t::epsilon, automaton),
                       bgl::get(&edge_bundle_t::transition, automaton),
                       bgl::get(&edge_bundle_t::occurences, automaton)));
  LOG(INFO) << "UICrashSummarization :=\n" << oss.str();
}

//
// WebSocketServer
//
WebSocketServer::WebSocketServer(FuzzerHandler &fuzzer_handler,
                                 const uint16_t port, const string &docroot_str,
                                 const string &data_store_str)
    : fuzzer_handler(fuzzer_handler), port(port) {
  try {
    initialize(docroot_str, data_store_str);
    fetch_init_info();
  } catch (exception &e) {
    LOG(ERROR) << "Exception- " << e.what();
  }
}

void WebSocketServer::initialize(const string &docroot_str,
                                 const string &data_store_str) {
  using websocketpp::lib::placeholders::_1;
  using websocketpp::lib::placeholders::_2;
  using websocketpp::lib::bind;

  docroot = fs::path(docroot_str);
  if (!fs::is_directory(docroot)) {
    LOG(ERROR) << "The docroot: " << docroot.string()
               << " is not a valid directory";
    errored = true;
    return;
  }

  data_store = std::unique_ptr<UIDataStore>(new UIDataStore(data_store_str));
  if (data_store->errored) {
    errored = true;
    LOG(ERROR) << "The database cannot be initialized";
    return;
  }

  store_graph = std::unique_ptr<UIStoreGraph>(
      new UIStoreGraph(fuzzer_handler.driver->knowledge->get_store()));

  crash_summary = std::unique_ptr<UICrashSummarization>(
      new UICrashSummarization(data_store, store_graph));

  crash_reader = std::unique_ptr<UICrashReader>(
      new UICrashReader(data_store, fuzzer_handler.vm["idir"].as<string>()));

  m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
  m_endpoint.clear_error_channels(websocketpp::log::alevel::all);

  LOG(INFO) << "Init websocket";

  m_endpoint.init_asio();
  m_endpoint.set_reuse_addr(true);

  m_endpoint.set_open_handler(bind(&WebSocketServer::on_open, this, _1));
  m_endpoint.set_close_handler(bind(&WebSocketServer::on_close, this, _1));

  m_endpoint.set_http_handler(bind(&WebSocketServer::on_http, this, _1));
  m_endpoint.set_message_handler(
      bind(&WebSocketServer::on_message, this, _1, _2));

  try {
    // m_endpoint.start_perpetual();
    m_endpoint.set_listen_backlog(port + 1);
    m_endpoint.listen(port);
    m_endpoint.start_accept();
    install_timer();

  } catch (exception const &e) {
    LOG(ERROR) << "Error binding the websocket server: " << e.what();
    m_endpoint.stop_listening();
    m_endpoint.stop();
    errored = true;
  }
}

void WebSocketServer::fetch_init_info() {
  fetch_global_info();
  fetch_target_info();
}

void WebSocketServer::fetch_global_info() {
  set<string> whitelist_options = {"script",
                                   "rand-seed",
                                   "population-initial-size",
                                   "population-deviation-size",
                                   "initial-buffer-size",
                                   "initial-buffer-deviation-size",
                                   "max-num-processes",
                                   "slow-mating-strategies",
                                   "evolution-timeout-seconds"};

  std::shared_ptr<GlobalMessage> global_message_ptr =
      std::make_shared<GlobalMessage>();
  global_message_ptr->command = fuzzer_handler.commander
                                    ? fuzzer_handler.commander->get_command()
                                    : "<None>";
  global_message_ptr->idir = fuzzer_handler.vm["idir"].as<string>();

  // Now, fetch all the options
  for (const auto &it : fuzzer_handler.vm) {
    const string key = it.first.c_str();
    if (whitelist_options.find(key) == whitelist_options.end())
      continue;
    string value_str;
    auto &value = it.second.value();

    if (auto v = boost::any_cast<bool>(&value)) {
      value_str = (*v ? "true" : "false");
    } else if (auto v = boost::any_cast<uint32_t>(&value)) {
      value_str = std::to_string(*v);
    } else if (auto v = boost::any_cast<std::string>(&value)) {
      value_str = *v;
    }

    if (!value_str.empty()) {
      global_message_ptr->options.insert({key, value_str});
    }
  }

  LOG(INFO) << "Options map: " << global_message_ptr->options;

  static_messages.insert({"global", global_message_ptr});
}

void WebSocketServer::fetch_target_info() {
  using namespace instr;

  std::shared_ptr<TargetMessage> target_message_ptr =
      std::make_shared<TargetMessage>();
  ProgramKnowledge *knowledge = fuzzer_handler.driver->knowledge.get();
  if (knowledge == nullptr)
    return;
  lookup_t &elements = knowledge->elements();

  uint32_t num_functions = 0, num_blocks = 0, num_goals = 0;
  for (const auto &m_elmt : elements) {

    target_message_ptr->elements.insert(
        {m_elmt.first, kindName(m_elmt.second->getKind())});

    switch (m_elmt.second->getKind()) {
    case Element::E_FUNCTION:
      num_functions++;
      break;
    case Element::E_BLOCK:
      num_blocks++;
      break;
    case Element::E_SUMMARY:
      num_goals++;
      break;
    default:
      break;
    }
  }

  target_message_ptr->num_functions = num_functions;
  target_message_ptr->num_blocks = num_blocks;
  target_message_ptr->num_goals = num_goals;

  static_messages.insert({"target", target_message_ptr});
}

WebSocketServer::~WebSocketServer() {
  m_endpoint.stop_listening();

  con_list::iterator it;
  for (auto &it : m_connections) {
    try {
      m_endpoint.close(it, websocketpp::close::status::normal, "");
    } catch (websocketpp::lib::error_code ec) {
      LOG(ERROR) << "lib::error_code " << ec;
    }
  }
  m_endpoint.stop();
}

bool WebSocketServer::start() {
  LOG(INFO) << "Start websocket";
  // Start the ASIO io_service run loop
  try {
    if (errored) {
      LOG(ERROR) << "Cannot start the websocket. No UI.";
      return false;
    }

    m_endpoint.run();
    return true;
  } catch (exception &e) {
    LOG(ERROR) << "Exception- " << e.what();
    m_endpoint.stop_listening();
    m_endpoint.stop();
    return false;
  }
}

void WebSocketServer::start_ticking() {
  if (should_tick == false) {
    install_timer();
    should_tick = true;
  }
}

void WebSocketServer::install_timer() {
  m_timer = m_endpoint.set_timer(
      1000, websocketpp::lib::bind(&WebSocketServer::on_tick, this,
                                   websocketpp::lib::placeholders::_1));
}

void WebSocketServer::on_tick(websocketpp::lib::error_code const &ec) {
  if (ec) {
    LOG(ERROR) << "Timer Error: " << ec.message();
    return;
  }

  // Gather current status and min/max scores, then buddle it into a JSON array
  /*
  ProgressMessage progress(
      fuzzer_handler.timer.seconds(),
      fuzzer_handler.generated_testcases.load());

    ScoreMessage score(
        fuzzer_handler.generated_testcases.load(),
        fuzzer_handler.driver->population->get_min_key_ro(),
         fuzzer_handler.driver->population->get_max_key_ro());

    CoverageMessage coverage(store_graph->max_coverage,
    store_graph->min_coverage,
                             store_graph->coverage);

    StatisticsMessage stats(
        fuzzer_handler.driver->population->get_current_stats(),
        fuzzer_handler.driver->population->get_best_ro(15));
  */

  Json::Value output = create_array();
  // output.append(progress.to_json());
  // output.append(score.to_json());
  // output.append(coverage.to_json());
  // output.append(stats.to_json());
  // output.append(get_reply_message_json("crashers", Json::Value()));

  // DEBUG ONLY
  // output.append(get_reply_message_json("functions", Json::Value()));

  const string output_str = to_json_string(output);

  // LOG(INFO) << "Tick info: " << output_str;

  // Broadcast count to all connections
  con_list::iterator it;
  for (it = m_connections.begin(); it != m_connections.end(); ++it) {
    m_endpoint.send(*it, output_str, websocketpp::frame::opcode::text);
  }

  // set timer for next message check
  install_timer();
}

void WebSocketServer::on_message(connection_hdl hdl, message_ptr msg) {
  // start_ticking();

  const string input = msg->get_payload();
  LOG(INFO) << "WS receives: " << input;
  try {
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(input, json)) {
      process_message(hdl, msg, json);
    }
  } catch (exception &e) {
    LOG(ERROR) << "Cannot process the message: " << input
               << " - Exception: " << e.what();
  }
}

void WebSocketServer::process_message(connection_hdl hdl, message_ptr msg,
                                      const Json::Value &json) {
  const string response = reply_message(json);
  if (!response.empty()) {
    LOG(INFO) << "WS sends: " << response;

    con_list::iterator it;
    for (it = m_connections.begin(); it != m_connections.end(); ++it) {
      m_endpoint.send(*it, response, msg->get_opcode());
    }
  }
}

//
// The kind of message we should receive are symmetrical to outputs:
//   msg :=
//     kind := message_kind (string)
//     data := array or object
//
string WebSocketServer::reply_message(const Json::Value &json) {
  if (json.empty() || !json.isObject() || json["kind"].isNull() ||
      !json["kind"].isString()) {
    LOG(ERROR) << "Cannot reply to wrongly constructed WS message: "
               << to_json_string(json);
    return string();
  }
  const string kind = json["kind"].asString();
  Json::Value reply = get_reply_message_json(kind, json["data"]);
  return to_json_string(reply);
}

Json::Value WebSocketServer::get_reply_message_json(const string &kind,
                                                    const Json::Value &data) {
  if (kind == "global") {
    return get_init_info();
  } else if (kind == "target") {
    return get_target_info();
  } else if (kind == "coverage") {
    return get_coverage_info();
  } else if (kind == "element_function" || kind == "element_block" ||
             kind == "element_goal") {
    instr::element_id id = get_json_id(data);
    if (id > 0) {
      return get_json_element(kind, id);
    }
  } else if (kind == "functions" || kind == "blocks" || kind == "goals") {
    const ServerMessage::MessageKind e_kind =
        (kind == "functions"
             ? ServerMessage::E_MESSAGE_ELEMENT_FUNCTION
             : (kind == "blocks" ? ServerMessage::E_MESSAGE_ELEMENT_BLOCK
                                 : ServerMessage::E_MESSAGE_ELEMENT_GOAL));
    return get_all_entities(e_kind);
  } else if (kind == "crashers") {
    return get_all_crashers();
  } else if (kind == "crash") {
    uint64_t testcase_id = get_json_id(data);
    if (testcase_id > 0) {
      return get_crash_info(testcase_id);
    }
  } else if (kind == "debug-loop") {
    string input = get_json_string(data);
    return get_evaluation_result(input);
  }
  return create_null_message(kind);
}

Json::Value WebSocketServer::get_json_element(const string &kind,
                                              const instr::element_id id) {
  if (!store_graph) {
    return create_null_message(kind);
  }

  auto iter = store_graph->messages.find(id);
  if (iter == store_graph->messages.end()) {
    return create_null_message(kind);
  }

  std::shared_ptr<ServerMessage> elmt = iter->second;
  switch (elmt->getKind()) {
  case ServerMessage::E_MESSAGE_ELEMENT_FUNCTION: {
    auto e = std::static_pointer_cast<ElementFunctionMessage>(elmt);
    return e->to_json();
  }
  case ServerMessage::E_MESSAGE_ELEMENT_BLOCK: {
    auto e = std::static_pointer_cast<ElementBlockMessage>(elmt);
    return e->to_json();
  }
  case ServerMessage::E_MESSAGE_ELEMENT_GOAL: {
    auto e = std::static_pointer_cast<ElementGoalMessage>(elmt);
    return e->to_json();
  }
  default:
    return create_null_message(kind);
  }
}

Json::Value WebSocketServer::get_init_info() {
  auto iter = static_messages.find("global");
  if (iter == static_messages.end() ||
      iter->second->getKind() != ServerMessage::E_MESSAGE_GLOBAL) {
    // Cannot find it, we don't send anything back to the client
    return create_null_message("global");
  }
  std::shared_ptr<GlobalMessage> global_message =
      std::static_pointer_cast<GlobalMessage>(iter->second);

  if (!global_message)
    return create_null_message("global");

  return global_message->to_json();
}

Json::Value WebSocketServer::get_target_info() {
  auto iter = static_messages.find("target");
  if (iter == static_messages.end() ||
      iter->second->getKind() != ServerMessage::E_MESSAGE_TARGET) {
    // Cannot find it, we don't send anything back to the client
    return create_null_message("target");
  }
  std::shared_ptr<TargetMessage> target_message =
      std::static_pointer_cast<TargetMessage>(iter->second);

  if (!target_message)
    return create_null_message("target");

  return target_message->to_json();
}

Json::Value WebSocketServer::get_coverage_info() {
  CoverageMessage coverage(store_graph->max_coverage, store_graph->min_coverage,
                           store_graph->coverage);
  return coverage.to_json();
}

// Iterate over the store and get the
Json::Value
WebSocketServer::get_all_entities(const ServerMessage::MessageKind kind) {
  auto json =
      create_initial_message("entities", /*error*/ false, /*array*/ true);
  json["data"] = Json::Value(Json::arrayValue);
  for (const auto &m_elmt : store_graph->messages) {
    if (m_elmt.second->getKind() == kind) {
      json["data"].append(m_elmt.second->to_json());
    }
  }
  return json;
}

Json::Value WebSocketServer::get_all_crashers() {
  CrashersMessage crashers_message(crash_reader->get_crash_mapping());
  return crashers_message.to_json();
}

Json::Value WebSocketServer::get_crash_info(const uint64_t testcase_id) {
  return crash_reader->get(testcase_id);
}

Json::Value
WebSocketServer::get_evaluation_result(const std::string &input_buffer) {
  const trace_score_t res = fuzzer_handler.isolated_evaluation(input_buffer);
  DebugLoopMessage debug_loop(input_buffer, res);
  return debug_loop.to_json();
}

void WebSocketServer::on_http(connection_hdl hdl) {
  // Upgrade our connection handle to a full connection_ptr
  server::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

  std::ifstream file;
  std::string filename = con->get_uri()->get_resource();
  std::string response;

  LOG(INFO) << "http request: " << filename;

  if (filename == "/") {
    filename = (docroot / "index.html").string();
  } else {
    filename = (docroot / filename.substr(1)).string();
  }

  fs::path filename_path(filename);
  if (!fs::is_regular_file(filename_path)) {
    LOG(INFO) << "Asking for non-existant file... redirect to index.html";
    filename = (docroot / "index.html").string();
  }

  LOG(INFO) << " [-] fetch to: " << filename;

  file.open(filename.c_str(), std::ios::in);
  file.seekg(0, std::ios::end);
  response.reserve(file.tellg());
  file.seekg(0, std::ios::beg);

  response.assign((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

  utils::replace_all(response, "COVERAGE_FUZZING_WS_PORT", std::to_string(port));
  utils::replace_all(response, "COVERAGE_FUZZING_HOST", "\"localhost\"");

  con->set_body(response);
  con->set_status(websocketpp::http::status_code::ok);
}

//
// ProgressMessage
//
Json::Value ProgressMessage::to_json() const {
  auto json = create_initial_message("progress");
  json["data"]["elapsed"] = elapsed_seconds;
  json["data"]["testcases"] = generated_testcases;
  return json;
}

//
// ScoreMessage
//
Json::Value ScoreMessage::to_json() const {
  auto json = create_initial_message("score");
  json["data"]["index"] = index;
  json["data"]["min"] = Json::Value();
  json["data"]["min"]["absolute"] =
      min_score.goal.absolute + min_score.edge.absolute;
  json["data"]["min"]["diff"] = min_score.goal.diff + min_score.edge.diff;

  json["data"]["max"] = Json::Value();
  json["data"]["max"]["absolute"] =
      max_score.goal.absolute + max_score.edge.absolute;
  json["data"]["max"]["diff"] = max_score.goal.diff + max_score.edge.diff;
  return json;
}

//
// GlobalMessage
//
Json::Value GlobalMessage::to_json() const {
  auto json = create_initial_message("global");
  json["data"]["command"] = command;
  json["data"]["idir"] = idir;
  json["data"]["options"] = Json::Value();
  for (auto &m_elmt : options) {
    json["data"]["options"][m_elmt.first] = m_elmt.second;
  }
  return json;
}

//
// ProgressMessage
//
Json::Value TargetMessage::to_json() const {
  auto json = create_initial_message("target");
  json["data"]["num_functions"] = num_functions;
  json["data"]["num_blocks"] = num_blocks;
  json["data"]["num_goals"] = num_goals;
  json["data"]["elements"] = Json::Value();
  for (auto &m_elmt : elements) {
    json["data"]["elements"][std::to_string(m_elmt.first)] = m_elmt.second;
  }
  return json;
}

//
// ElementFunctionMessage
//
Json::Value ElementFunctionMessage::to_json() const {
  auto json = create_initial_message("element_function");
  json["data"]["id"] = id;
  json["data"]["name"] = name;
  json["data"]["blocks"] = Json::Value(Json::arrayValue);
  for (auto block_id : blocks) {
    json["data"]["blocks"].append(block_id);
  }
  return json;
}

//
// ElementFunctionMessage
//
Json::Value ElementBlockMessage::to_json() const {
  auto json = create_initial_message("element_block");
  json["data"]["id"] = id;
  json["data"]["block_id"] = block_id;
  json["data"]["goals"] = Json::Value(Json::arrayValue);
  for (auto goal_id : goals) {
    json["data"]["goals"].append(goal_id);
  }
  return json;
}

//
// ElementFunctionMessage
//
Json::Value ElementGoalMessage::to_json() const {
  auto json = create_initial_message("element_goal");
  json["data"]["id"] = id;
  json["data"]["type"] = type;
  json["data"]["operator"] = op;
  return json;
}

//
// CoverageMessage
//
Json::Value CoverageMessage::to_json() const {
  auto json = create_initial_message("coverage");
  json["data"]["min"] = min_coverage;
  json["data"]["max"] = max_coverage;
  json["data"]["coverage"] = Json::Value();
  for (auto &m_elmt : coverage) {
    json["data"]["coverage"][std::to_string(m_elmt.first)] = m_elmt.second;
  }
  return json;
}

//
// CrashersMessage
//
Json::Value CrashersMessage::to_json() const {
  auto json = create_initial_message("crashers");
  json["data"]["crash_ids"] = Json::Value();
  for (auto &m_elmt : crash_ids) {
    json["data"]["crash_ids"][m_elmt.first] = Json::Value(Json::arrayValue);
    for (auto &x : m_elmt.second) {
      json["data"]["crash_ids"][m_elmt.first].append(x);
    }
  }
  return json;
}

//
// DebugLoopMessage
//
Json::Value DebugLoopMessage::to_json() const {
  auto json = create_initial_message("debug-loop");
  json["data"]["input"] = input;
  json["data"]["score"] = from_measure(trace_score.second);

  json["data"]["trace"] = Json::Value(Json::arrayValue);
  for (auto &x : trace_score.first) {
    json["data"]["trace"].append(x);
  }
  return json;
}

//
// StatisticsMessage::SimpleTestCase
//
Json::Value StatisticsMessage::SimpleTestCase::to_json() const {
  auto json = Json::Value();
  json["position"] = position;
  json["length"] = length;
  json["repr"] = Json::Value(Json::arrayValue);

  vector<string> repr_lines;
  boost::split(repr_lines, repr, boost::is_any_of("\n\r"),
               boost::token_compress_on);
  for (auto &x : repr_lines) {
    json["repr"].append(x);
  }

  json["score"] = from_measure(score);
  return json;
}

//
// StatisticsMessage
//
Json::Value StatisticsMessage::to_json() const {
  auto json = create_initial_message("stats");
  json["data"]["stats"] = Json::Value();
  for (auto &m_elmt : stats) {
    json["data"]["stats"][m_elmt.first] = m_elmt.second;
  }

  json["data"]["best"] = Json::Value(Json::arrayValue);
  for (auto &x : best) {
    json["data"]["best"].append(x.to_json());
  }
  return json;
}

// end namespace=ui
}
// end namespace=fuzz
}
