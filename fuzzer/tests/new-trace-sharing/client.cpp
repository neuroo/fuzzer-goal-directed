#include "client.h"
namespace fuzz {

namespace shared {

client_t::client_t(const std::string &ipc_endpoint)
    : endpoint(ipc_endpoint), socket(io_service) {
  socket.connect(endpoint);
  send("");
}

client_t::~client_t() { io_service.stop(); }

void client_t::send(const std::string &msg) {
  if (msg.empty())
    return;

  boost::asio::async_write(socket, asio::buffer(msg),
                           boost::bind(&client_t::handle_write,
                                       shared_from_this(),
                                       asio::placeholders::error));
}

void client_t::handle_write(const boost::system::error_code &error) {
  if (error)
    return;
  send("");
}

} // namespace=shared

} // namespace=fuzz
