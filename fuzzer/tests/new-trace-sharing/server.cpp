#include "server.h"

#include <iostream>

namespace fuzz {

namespace shared {

server_t::server_t(const std::string &ipc_endpoint)
    : endpoint(ipc_endpoint), acceptor(io_service, endpoint),
      socket(io_service), service_thread(&server_t::run_service, this) {
  socket.bind(endpoint);
}

server_t::~server_t() {
  io_service.stop();
  service_thread.join();
}

void server_t::run_service() {
  start_receive();
  while (!io_service.stopped()) {
    try {
      io_service.run();
    } catch (const std::exception &e) {
      std::cerr << "Server: Network exception: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Server: Network exception: unknown" << std::endl;
    }
  }
}

void server_t::start_receive() {
  /*
  socket.async_receive_from(
      asio::buffer(recv_buffer), endpoint,
      [this](boost::system::error_code ec, std::size_t bytes_recvd) {
        this->handle_receive(ec, bytes_recvd);
      });*/

  boost::asio::async_read(socket, asio::buffer(recv_buffer),
                          boost::bind(&server_t::handle_receive, this,
                                      asio::placeholders::error,
                                      asio::placeholders::bytes_transferred));
}

void server_t::handle_receive(const boost::system::error_code &error,
                              std::size_t bytes_transferred) {
  if (!error) {
    try {
      auto message = std::string(recv_buffer.data(),
                                 recv_buffer.data() + bytes_transferred);
      if (!message.empty()) {
        std::cout << "received: " << message << std::endl;
      }

    } catch (std::exception ex) {
      std::cerr << "handle_receive: Error parsing incoming message: "
                << ex.what() << std::endl;
    } catch (...) {
      std::cerr
          << "handle_receive: Unknown error while parsing incoming message"
          << std::endl;
    }
  } else {
    std::cerr << "handle_receive: error: " << error.message() << std::endl;
  }

  start_receive();
}

} // namespace=shared

} // namespace=fuzz
