#ifndef UI_H
#define UI_H

#include "common/elements.h"
#include "handler.h"
#include "knowledge.h"
#include "ui-messages.h"
#include "utils.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/property_map.hpp>
namespace bgl = boost;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "json/json.h"

#include "leveldb/db.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace fuzz {

namespace ui {

struct UIDataStore;

//
// UICrashReader
//
struct UICrashReader {
  std::unique_ptr<UIDataStore> &data_store;

  fs::path idir;
  fs::path crashes;
  fs::path results;

  std::mutex crashes_mutex;
  std::set<std::string> locked_crash_ids;
  std::map<std::string, std::set<uint64_t>> crashes_mapping;

  std::mutex bimap_mutex;
  std::vector<std::string> crash_ids;
  std::map<uint64_t, size_t> bimap;

  UICrashReader() = delete;
  UICrashReader(const UICrashReader &) = delete;
  UICrashReader &operator=(const UICrashReader &) = delete;

  UICrashReader(std::unique_ptr<UIDataStore> &data_store,
                const std::string &idir_str);

  void run();

  // For a given testcase that caused a crash, retrieve all
  // information. That means read from the data_store when it's
  // ready, then read the JSON from disk with the summary of
  // the crash.
  Json::Value get(const uint64_t testcase_id);

  std::set<uint64_t> get_crashers();
  std::set<std::string> get_crash_ids();
  std::map<std::string, std::set<uint64_t>> get_crash_mapping();

private:
  void initialize(const std::string &idir_str);
  void read_crash_dir(const std::string &crash_id, const fs::path &dir);
  bool associated_bimap(const uint64_t testcase_id,
                        const std::string &crash_id);

  std::string get_crash_id(const uint64_t testcase_id);
};

//
// UIDataStore essentially a file-based cache to store data computed
// by the fuzzer
//
struct UIDataStore {
  fs::path store_path;
  leveldb::Options options;
  leveldb::DB *db = nullptr;
  bool errored = false;

  UIDataStore() = delete;
  UIDataStore(const UIDataStore &) = delete;
  UIDataStore &operator=(const UIDataStore &) = delete;

  UIDataStore(const std::string &store_path_str);
  ~UIDataStore();

  bool set(const uint64_t tescase_id, const ga::ShareableIndividual &ind);
  std::string get(const uint64_t tescase_id, bool &fetch_error) const;

private:
  void initialize(const std::string &store_path_str);

  std::string to_key(const uint64_t tescase_id) const;
  std::string to_repr(const ga::ShareableIndividual &ind) const;
  std::string from_rep(const std::string &rep, bool &fetch_error) const;
};

//
// UIStoreGraph captures our Store in a graph and makes it easier to quickly
// retrieve the information for the UI
//
struct UIStoreGraph {
  // A bundled property for our vertex is the element_id this vertex
  // points to. Vertices are basic blocks.
  struct vertex_bundle_t {
    instr::element_id element;
  };

  // In our graph, we store vertices and edges in a vector. That's actually
  // better for a more compact representation of the graph.
  typedef bgl::adjacency_list<bgl::vecS, bgl::vecS, bgl::bidirectionalS,
                              vertex_bundle_t>
      graph_t;
  typedef bgl::graph_traits<graph_t>::vertex_descriptor vertex_t;
  typedef bgl::graph_traits<graph_t>::vertex_iterator vertex_iterator;
  typedef bgl::graph_traits<graph_t>::edge_descriptor edge_t;
  typedef bgl::graph_traits<graph_t>::edge_iterator edge_iterator;

  std::map<instr::element_id, vertex_t> vertices;
  std::map<instr::element_id, std::shared_ptr<ServerMessage>> messages;
  graph_t graph;

  uint32_t max_coverage = 0;
  uint32_t min_coverage = 0;
  // Element ID -> number of time it's covered
  coverage_t coverage;

  UIStoreGraph() = delete;
  UIStoreGraph(const UIStoreGraph &) = delete;
  UIStoreGraph &operator=(const UIStoreGraph &) = delete;

  UIStoreGraph(const instr::Store *store);
  ~UIStoreGraph();

  void update_coverage(const coverage_t &local_coverage);

private:
  void initialize(const instr::Store *store);

  void to_dot();

  void
  handle_function(const instr::StoreImpl &s_impl,
                  const std::shared_ptr<instr::FunctionElement> &func_elmt);

  void handle_block(const instr::StoreImpl &s_impl, const vertex_t &func_vertex,
                    const std::shared_ptr<instr::BlockElement> &block_elmt);

  void handle_goal(const instr::StoreImpl &s_impl, const vertex_t &block_vertex,
                   const std::shared_ptr<instr::SummaryElement> &summary_elmt);
};

//
// UICrashSummarization
//
struct UICrashSummarization {
  std::unique_ptr<UIDataStore> &data_store;
  std::unique_ptr<UIStoreGraph> &store_graph;

  struct vertex_bundle_t {
    bool start = false;
    bool end = false;
    uint32_t index = 0;
  };

  struct edge_bundle_t {
    bool epsilon = false;
    uint8_t transition;
    uint64_t occurences = 0;
    std::string crash_id;
  };

  template <class EpsilonMap, class TransitionMap, class OccurencesMap>
  class edge_writer {
    EpsilonMap em;
    TransitionMap tm;
    OccurencesMap om;

  public:
    edge_writer(EpsilonMap e, TransitionMap t, OccurencesMap o)
        : em(e), tm(t), om(o) {}

    template <class Edge>
    void operator()(std::ostream &out, const Edge &e) const {
      if (em[e]) {
        out << "[label=\"&epsilon;\"]";
      } else {
        out << "[label=\"" << utils::html_escape(tm[e]) << ", N=" << om[e]
            << "\"]";
      }
    }
  };

  template <class EpsilonMap, class TransitionMap, class OccurencesMap>
  inline edge_writer<EpsilonMap, TransitionMap, OccurencesMap>
  make_edge_writer(EpsilonMap e, TransitionMap t, OccurencesMap o) {
    return edge_writer<EpsilonMap, TransitionMap, OccurencesMap>(e, t, o);
  }

  // In our graph, we store vertices and edges in a vector. That's actually
  // better for a more compact representation of the graph.
  typedef bgl::adjacency_list<bgl::vecS, bgl::vecS, bgl::undirectedS,
                              vertex_bundle_t, edge_bundle_t>
      automaton_t;
  typedef bgl::graph_traits<automaton_t>::vertex_descriptor vertex_t;
  typedef bgl::graph_traits<automaton_t>::vertex_iterator vertex_iterator;
  typedef bgl::graph_traits<automaton_t>::edge_descriptor edge_t;
  typedef bgl::graph_traits<automaton_t>::edge_iterator edge_iterator;

  vertex_t start;
  vertex_t end;
  std::map<uint32_t, vertex_t> vertices;
  std::map<std::pair<vertex_t, uint8_t>, edge_t> edges;
  automaton_t automaton;

  std::set<uint64_t> delay_processing;

  UICrashSummarization() = delete;
  UICrashSummarization(const UICrashSummarization &) = delete;
  UICrashSummarization &operator=(const UICrashSummarization &) = delete;

  UICrashSummarization(std::unique_ptr<UIDataStore> &data_store,
                       std::unique_ptr<UIStoreGraph> &store_graph);

  void update_crashers(const std::set<uint64_t> &crashers_id);

  void to_dot();

private:
  void insert_crasher(const uint64_t testcase_id, std::string &buffer);
};

//
// Actual websocket server
//
struct WebSocketServer {
  typedef websocketpp::connection_hdl connection_hdl;
  typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;
  typedef websocketpp::server<websocketpp::config::asio> server;
  typedef websocketpp::lib::lock_guard<websocketpp::lib::mutex> scoped_lock;
  typedef server::message_ptr message_ptr;

  FuzzerHandler &fuzzer_handler;
  uint16_t port;
  bool errored = false;
  bool should_tick = false;

  // Essentially all of our managed data...
  std::unique_ptr<UIDataStore> data_store;
  std::unique_ptr<UIStoreGraph> store_graph;
  std::unique_ptr<UICrashSummarization> crash_summary;
  std::unique_ptr<UICrashReader> crash_reader;

  std::map<std::string, std::shared_ptr<ServerMessage>> static_messages;

  server m_endpoint;
  con_list m_connections;
  server::timer_ptr m_timer;
  fs::path docroot;

  WebSocketServer() = delete;
  WebSocketServer(const WebSocketServer &) = delete;
  WebSocketServer &operator=(const WebSocketServer &) = delete;

  WebSocketServer(FuzzerHandler &fuzzer_handler, const uint16_t port,
                  const std::string &docroot_str,
                  const std::string &store_path_str);

  ~WebSocketServer();

  bool start();

  void start_ticking();

private:
  void initialize(const std::string &docroot_str,
                  const std::string &store_path_str);

  void on_message(connection_hdl hdl, message_ptr msg);
  void on_tick(websocketpp::lib::error_code const &ec);

  void on_http(connection_hdl hdl);
  void on_open(connection_hdl hdl) { m_connections.insert(hdl); }
  void on_close(connection_hdl hdl) { m_connections.erase(hdl); }

  void install_timer();

  void fetch_init_info();
  void fetch_global_info();
  void fetch_target_info();

  void process_message(connection_hdl hdl, message_ptr msg,
                       const Json::Value &json);
  std::string reply_message(const Json::Value &json);
  Json::Value get_reply_message_json(const std::string &kind,
                                     const Json::Value &data);

  Json::Value get_init_info();
  Json::Value get_target_info();
  Json::Value get_coverage_info();
  Json::Value get_all_entities(const ServerMessage::MessageKind kind);
  Json::Value get_all_crashers();
  Json::Value get_crash_info(const uint64_t testcase_id);
  Json::Value get_evaluation_result(const std::string &input_buffer);
  Json::Value get_json_element(const std::string &kind,
                               const instr::element_id id);
};

// end namespace=ui
}
// end namespace=fuzz
}

#endif
