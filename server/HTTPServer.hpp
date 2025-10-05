#ifndef VIX_HTTP_SERVER_HPP
#define VIX_HTTP_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include "../router/Router.hpp"
#include "../session/Session.hpp"
#include "../config/Config.hpp"
#include "ThreadPool.hpp"

namespace Vix
{
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    constexpr size_t NUMBER_OF_THREADS = 8;

    class HTTPServer
    {
    public:
        explicit HTTPServer(Config &config);
        ~HTTPServer();

        void run();
        void start_accept();
        int calculate_io_thread_count();
        std::shared_ptr<Router> getRouter() { return router_; }
        void monitor_metrics();
        void stop_async();
        void join_threads();

    private:
        void init_acceptor(unsigned short port);
        void handle_client(std::shared_ptr<tcp::socket> socket_ptr, std::shared_ptr<Router> router);
        void close_socket(std::shared_ptr<tcp::socket> socket);

        void start_io_threads();

        Config &config_;
        std::shared_ptr<net::io_context> io_context_;
        std::unique_ptr<tcp::acceptor> acceptor_;
        std::shared_ptr<Router> router_;
        Vix::ThreadPool request_thread_pool_;
        std::vector<std::thread> io_threads_;
        std::atomic<bool> stop_requested_;
    };

    void set_affinity(int thread_id);
}

#endif // VIX_HTTP_SERVER_HPP
