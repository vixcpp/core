#include "HTTPServer.hpp"
#include "../utils/Logger.hpp"

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

    void HTTPServer::init_acceptor(unsigned short port)
    {
        auto &log = Vix::Logger::getInstance();

        auto check_error = [&log](const boost::system::error_code &ec, const std::string &msg)
        {
            if (ec)
            {
                log.log(Vix::Logger::Level::ERROR, "{}: {} (Error code: {})", msg, ec.message(), ec.value());
                throw std::system_error(ec, msg);
            }
        };

        tcp::endpoint endpoint(net::ip::tcp::v4(), port);
        acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);

        boost::system::error_code ec;

        acceptor_->open(endpoint.protocol(), ec);
        check_error(ec, "Could not open the acceptor socket");

        acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
        check_error(ec, "Could not set socket option");

        acceptor_->bind(endpoint, ec);
        check_error(ec, fmt::format("Could not bind to the server port {}", port));

        acceptor_->listen(boost::asio::socket_base::max_connections, ec);
        check_error(ec, fmt::format("Could not listen on the server port {}", port));

        log.log(Vix::Logger::Level::INFO, "Acceptor initialized on port {}", port);
    }

    HTTPServer::HTTPServer(Config &config)
        : config_(config),
          io_context_(std::make_shared<net::io_context>()),
          acceptor_(nullptr),
          router_(std::make_shared<Router>()),
          route_configurator_(std::make_unique<RouteConfigurator>(*router_)),
          request_thread_pool_(NUMBER_OF_THREADS, 100, 0, std::chrono::milliseconds(1000)),
          io_threads_(),
          stop_requested_(false)
    {
        auto &log = Vix::Logger::getInstance();
        try
        {
            int port = config_.getServerPort();
            if (port < 1024 || port > 65535)
            {
                log.log(Vix::Logger::Level::ERROR, "Section port {} out of range (1024-65535)", port);
                throw;
            }

            init_acceptor(static_cast<unsigned short>(port));
        }
        catch (const std::exception &e)
        {
            log.log(Vix::Logger::Level::ERROR, "Error initializing HTTPServer: {}", e.what());
            throw;
        }
    }

    HTTPServer::~HTTPServer() {}

    void HTTPServer::run()
    {
        auto &log = Vix::Logger::getInstance();

        try
        {
            route_configurator_->configure_routes();

            log.log(Vix::Logger::Level::INFO,
                    "Vix is running at http://127.0.0.1:{} using {} threads",
                    config_.getServerPort(), NUMBER_OF_THREADS);
            log.log(Vix::Logger::Level::INFO, "Waiting for incoming connections...");

            start_accept();

            std::size_t num_io_threads = calculate_io_thread_count();
            log.log(Vix::Logger::Level::INFO, "Starting {} io_context threads", num_io_threads);

            for (std::size_t i = 0; i < num_io_threads; ++i)
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
                        log.log(Vix::Logger::Level::ERROR,
                                "Error in io_context (thread {}): {}", i, e.what());
                    }
                    log.log(Vix::Logger::Level::INFO, "Thread {} finished.", i); });
            }

            for (auto &t : io_threads_)
            {
                if (t.joinable())
                    t.join();
            }

            log.log(Vix::Logger::Level::INFO, "All io_context threads finished.");
        }
        catch (const std::exception &e)
        {
            log.log(Vix::Logger::Level::ERROR, "Error in HTTPServer::run(): {}", e.what());
        }
    }

    int HTTPServer::calculate_io_thread_count()
    {
        unsigned int hc = std::thread::hardware_concurrency();
        return std::max(1, static_cast<int>(hc > 0 ? hc / 2 : 1));
    }

    void HTTPServer::start_accept()
    {
        auto socket = std::make_shared<tcp::socket>(*io_context_);
        auto &log = Vix::Logger::getInstance();

        try
        {
            acceptor_->async_accept(
                *socket,
                [this, socket, &log](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        request_thread_pool_.enqueue(1, [this, socket, &log]()
                                                     {
                        try {
                            handle_client(socket, router_);
                        } 
                        catch (const std::exception &e) {
                            log.log(Vix::Logger::Level::ERROR, "Error handling client: {}", e.what());
                            close_socket(socket);
                        } 
                        catch (...) {
                            log.log(Vix::Logger::Level::ERROR, "Unknown error handling client");
                            close_socket(socket);
                        } });
                    }
                    else
                    {
                        log.log(Vix::Logger::Level::ERROR,
                                "Error accepting connection from client: {} (Error code: {})", ec.message(), ec.value());
                    }

                    start_accept();
                });
        }
        catch (const std::exception &e)
        {
            log.log(Vix::Logger::Level::ERROR, "Exception during async_accept: {}", e.what());
            acceptor_->close();
        }
        catch (...)
        {
            log.log(Vix::Logger::Level::ERROR, "Unknown exception during async_accept");
            acceptor_->close();
        }
    }

    void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
    {
        auto &log = Vix::Logger::getInstance();
        boost::system::error_code ec;

        socket->shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::system::error_code{})
        {
            log.log(Vix::Logger::Level::ERROR,
                    "Failed to shutdown socket: {} (Error code: {})", ec.message(), ec.value());
        }

        socket->close(ec);
        if (ec && ec != boost::system::error_code{})
        {
            log.log(Vix::Logger::Level::ERROR,
                    "Failed to close socket: {} (Error code: {})", ec.message(), ec.value());
        }
    }

    void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr, std::shared_ptr<Router> router)
    {
        auto &log = Vix::Logger::getInstance();

        try
        {
            auto session = std::make_shared<Session>(socket_ptr, *router);
            session->run();
        }
        catch (const std::exception &e)
        {
            log.log(Vix::Logger::Level::ERROR,
                    "Error in client session for client {}: {}",
                    socket_ptr->remote_endpoint().address().to_string(), e.what());
            close_socket(socket_ptr);
        }
        catch (...)
        {
            log.log(Vix::Logger::Level::ERROR,
                    "Unknown error in client session for client {}",
                    socket_ptr->remote_endpoint().address().to_string());
            close_socket(socket_ptr);
        }
    }

}
