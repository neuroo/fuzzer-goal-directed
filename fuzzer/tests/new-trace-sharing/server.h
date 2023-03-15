#ifndef __SERVER_H
#define __SERVER_H

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
namespace asio = boost::asio;

namespace fuzz {

namespace shared {

struct server_t {
  asio::io_service io_service;
  asio::local::stream_protocol::endpoint endpoint;
  asio::local::stream_protocol::acceptor acceptor;
  asio::local::stream_protocol::socket socket;
  boost::thread service_thread;
  boost::array<uint8_t, 4096> recv_buffer;

  server_t(const std::string &ipc_endpoint);
  server_t(const server_t &) = delete;
  server_t &operator=(const server_t &) = delete;
  ~server_t();

  void run_service();
  void start_receive();
  void handle_receive(const boost::system::error_code &error,
                      std::size_t bytes_transferred);
};

} // namespace=shared

} // namespace=fuzz

#endif
