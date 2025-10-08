#include <vix/server/HTTPServer.hpp>
#include <vix/utils/Logger.hpp>

namespace Vix
{
    void set_affinity(int thread_id)
    {
#ifdef __linux__
        unsigned int hc = std::thread::hardware_concurrency();
        if (hc == 0)
            hc = 1;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(thread_id % hc, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    }

    HTTPServer::HTTPServer(Config &config)
        : config_(config),
          io_context_(std::make_shared<net::io_context>()),
          acceptor_(nullptr),
          router_(std::make_shared<Router>()),
          request_thread_pool_(NUMBER_OF_THREADS, 100, 0, 4),
          io_threads_(),
          stop_requested_(false)
    {
        auto &log = Logger::getInstance();
        try
        {
            int port = config_.getServerPort();
            if (port < 1024 || port > 65535)
            {
                log.log(Logger::Level::ERROR, "Server port {} out of range (1024-65535)", port);
                throw std::invalid_argument("Invalid port number");
            }

            init_acceptor(static_cast<unsigned short>(port));

            log.log(Logger::Level::INFO,
                    "Server request timeout set to {} ms",
                    config_.getRequestTimeout());
        }
        catch (const std::exception &e)
        {
            log.log(Logger::Level::ERROR, "Error initializing HTTPServer: {}", e.what());
            throw;
        }
    }

    HTTPServer::~HTTPServer() = default;

    void HTTPServer::init_acceptor(unsigned short port)
    {
        auto &log = Logger::getInstance();
        acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);
        boost::system::error_code ec;

        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_->open(endpoint.protocol(), ec);
        if (ec)
            throw std::system_error(ec, "open acceptor");
        acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            throw std::system_error(ec, "reuse_address");
        acceptor_->bind(endpoint, ec);
        if (ec)
            throw std::system_error(ec, "bind acceptor");
        acceptor_->listen(boost::asio::socket_base::max_connections, ec);
        if (ec)
            throw std::system_error(ec, "listen acceptor");

        log.log(Logger::Level::INFO, "Acceptor initialized on port {}", port);
    }

    void HTTPServer::start_io_threads()
    {
        auto &log = Logger::getInstance();
        std::size_t num_threads = calculate_io_thread_count();

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            io_threads_.emplace_back(
                [this, i, &log]()
                {
                try
                {
                    set_affinity(i);
                    io_context_->run();
                }
                catch (const std::exception &e)
                {
                    log.log(Logger::Level::ERROR, "Error in io_context thread {}: {}", i, e.what());
                }
                log.log(Logger::Level::INFO, "IO thread {} finished", i); });
        }
    }

    void HTTPServer::run()
    {
        start_accept();
        monitor_metrics();
        start_io_threads();
    }

    int HTTPServer::calculate_io_thread_count()
    {
        unsigned int hc = std::thread::hardware_concurrency();
        return std::max(1, static_cast<int>(hc > 0 ? hc / 2 : 1));
    }

    void HTTPServer::start_accept()
    {
        auto socket = std::make_shared<tcp::socket>(*io_context_);
        acceptor_->async_accept(*socket, [this, socket](boost::system::error_code ec)
                                {
            if (!ec && !stop_requested_)
            {
                auto timeout = std::chrono::milliseconds(config_.getRequestTimeout());
                request_thread_pool_.enqueue(1, timeout, [this, socket]()
                                            { handle_client(socket, router_); });
            }
            if (!stop_requested_) start_accept(); });
    }

    void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr, std::shared_ptr<Router> router)
    {
        auto session = std::make_shared<Session>(socket_ptr, *router);
        session->run();
    }

    void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
    {
        boost::system::error_code ec;
        socket->shutdown(tcp::socket::shutdown_both, ec);
        socket->close(ec);
    }

    void HTTPServer::stop_async()
    {
        stop_requested_ = true;
        if (acceptor_ && acceptor_->is_open())
            acceptor_->close();
        io_context_->stop();
    }

    void HTTPServer::join_threads()
    {
        for (auto &t : io_threads_)
            if (t.joinable())
                t.join();
    }

    void HTTPServer::monitor_metrics()
    {
        request_thread_pool_.periodicTask(0, [this]()
                                          {
            auto metrics = request_thread_pool_.getMetrics();
            Logger::getInstance().log(Logger::Level::INFO,
                "ThreadPool Metrics -> Pending: {}, Active: {}, TimedOut: {}",
                metrics.pendingTasks, metrics.activeTasks, metrics.timedOutTasks); }, std::chrono::seconds(5));
    }
}
