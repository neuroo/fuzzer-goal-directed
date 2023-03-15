#ifndef __CLIENT_H
#define __CLIENT_H

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
namespace asio = boost::asio;

namespace fuzz {

namespace shared {

struct client_t : public boost::enable_shared_from_this<client_t> {
  asio::io_service io_service;
  asio::local::stream_protocol::endpoint endpoint;
  asio::local::stream_protocol::socket socket;

  client_t(const std::string &ipc_endpoint);
  client_t(const client_t &) = delete;
  client_t &operator=(const client_t &) = delete;
  ~client_t();

  void run_service();
  void send(const std::string &msg);
  void handle_write(const boost::system::error_code &error);
};

} // namespace=shared

} // namespace=fuzz

#endif
