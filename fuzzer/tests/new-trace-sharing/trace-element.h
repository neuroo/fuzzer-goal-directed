#ifndef TRACE_ELEMENT_H
#define TRACE_ELEMENT_H

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

#include <cereal/archives/binary.hpp>

#include <sstream>
#include <string>
#include <vector>

#include "shared-data/shared-data.h"
using namespace shm;
/*
#include "CpperoMQ/All.hpp"
*/
struct SendableTraceElement {
  TraceKind kind = E_UNKNOWN;
  uint64_t thread_id = 0;
  uint32_t func_id = 0;
  uint32_t pred_block_id = 0; // no polymorphism, we can just add these
  uint32_t cur_block_id = 0;  // two integers all the time

  SendableTraceElement() = default;
  ~SendableTraceElement() = default;
  SendableTraceElement(const TraceKind kind, const uint64_t thread_id = 0,
                       const uint32_t func_id = 0,
                       const uint32_t pred_block_id = 0,
                       const uint32_t cur_block_id = 0)
      : kind(kind), thread_id(thread_id), func_id(func_id),
        pred_block_id(pred_block_id), cur_block_id(cur_block_id) {}

  SendableTraceElement(const SendableTraceElement &e)
      : kind(e.kind), thread_id(e.thread_id), func_id(e.func_id),
        pred_block_id(e.pred_block_id), cur_block_id(e.cur_block_id) {}

  SendableTraceElement &operator=(const SendableTraceElement &e) {
    if (&e != this) {
      kind = e.kind;
      thread_id = e.thread_id;
      func_id = e.func_id;
      pred_block_id = e.pred_block_id;
      cur_block_id = e.cur_block_id;
    }
    return *this;
  }

  std::string toString() const {
    ostringstream oss;
    oss << "<SendableTraceElement "
        << " thread_id=" << thread_id << " func_id=" << func_id;
    if (pred_block_id > 0) {
      oss << " " << pred_block_id << "->" << cur_block_id;
    }
    oss << ">";
    return oss.str();
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(kind, thread_id, func_id, pred_block_id, cur_block_id);
  }
};

struct SendableTrace {
  uint64_t testcase_id;
  std::vector<SendableTraceElement> elements;

  SendableTrace() = default;
  SendableTrace(const uint64_t testcase_id,
                const std::vector<SendableTraceElement> &elements)
      : testcase_id(testcase_id), elements(elements) {}
  SendableTrace(const SendableTrace &s)
      : testcase_id(s.testcase_id), elements(s.elements) {}

  SendableTrace &operator=(const SendableTrace &t) {
    if (&t != this) {
      testcase_id = t.testcase_id;
      elements = t.elements;
    }
    return *this;
  }

  void add(const SendableTraceElement &e) { elements.push_back(e); }

  template <class Archive> void serialize(Archive &archive) {
    archive(testcase_id, elements);
  }
};

// Utils
std::string hex_dump(const char *data, size_t size);
bool deserializeFromArhive(const std::string &input,
                           SendableTrace &sendable_trace);
bool serializeFromArchive(const SendableTrace &sendable_trace,
                          std::string &str);

// Container
struct SendatableTraceContainer /* : public CpperoMQ::Sendable,
                                  public CpperoMQ::Receivable */ {
  SendableTrace sendable_trace;

  SendatableTraceContainer() = default;
  SendatableTraceContainer(const SendatableTraceContainer &s)
      : sendable_trace(s.sendable_trace){};
  SendatableTraceContainer &operator=(const SendatableTraceContainer &s) {
    if (&s != this) {
      sendable_trace = s.sendable_trace;
    }
    return *this;
  }

  void set_id(const uint64_t testcase_id) {
    sendable_trace.testcase_id = testcase_id;
  }
  void add(const SendableTraceElement &e) { sendable_trace.add(e); }
  /*
    virtual bool send(const CpperoMQ::Socket &socket,
                      const bool moreToSend) const override {
      std::string input;
      serializeFromArchive(sendable_trace, input);
      CpperoMQ::OutgoingMessage msg_part(input.length(), input.c_str());
      if (!msg_part.send(socket, moreToSend))
        return false;
      return true;
    }

    virtual bool receive(CpperoMQ::Socket &socket, bool &moreToReceive) override
    {
      CpperoMQ::IncomingMessage msg_part;
      if (!msg_part.receive(socket, moreToReceive))
        return false;

      std::string str;
      str.assign(msg_part.charData(), msg_part.size());
      return deserializeFromArhive(str, sendable_trace);
    }*/
};

#endif
