#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#define BOOST_DISABLE_ASSERTS

#include <boost/asio.hpp>
#include <boost/atomic.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <cstdlib>

namespace threading {

class threadpool_t {
  using asio_worker = std::unique_ptr<boost::asio::io_service::work>;

  boost::asio::io_service service;
  asio_worker working;
  boost::thread_group threads;

  uint32_t n_threads;
  mutable boost::atomic<uint32_t> n_avail;

public:
  threadpool_t(uint32_t);
  ~threadpool_t();

  template <class F> void enqueue(F f);

  bool is_full() const { return n_avail < 1; }
  void one_busy() { --n_avail; }
  void one_avail() { ++n_avail; }
};

threadpool_t::threadpool_t(uint32_t _n_threads)
    : service(), working(new asio_worker::element_type(service)),
      n_threads(_n_threads), n_avail(_n_threads) {

  for (uint32_t i = 0; i < n_threads; ++i) {
    threads.create_thread(boost::bind(&boost::asio::io_service::run, &service));
  }
}

template <class F> void threadpool_t::enqueue(F f) {
  try {
    one_busy();
    service.post(f);
  } catch (...) {
    //
  }
}

threadpool_t::~threadpool_t() {
  try {
    working.reset();
    service.stop();
  } catch (...) { /**/
  }
}
}

#endif
