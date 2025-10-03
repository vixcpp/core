#include "HTTPServer.hpp"

namespace Vix
{
    void set_affinity(int thread_id)
    {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(thread_id % std::thread::hardware_concurrency(), &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    }

    HTTPServer::HTTPServer(Config &config)
        : config_(config),
          io_context_(std::make_shared<net::io_context>()),
          acceptor_(nullptr),
          router_(),
          route_configurator_(std::make_unique<RouteConfigurator>(router_)),
          request_thread_pool_(NUMBER_OF_THREADS, 100, 0, std::chrono::milliseconds(1000)),
          io_threads_(),
          stop_requested_(false)
    {
        try
        {
            int newPort = config_.getServerPort();
            if (newPort < 1024 || newPort > 65535)
            {
                throw std::invalid_argument("Port number out of range (1024-65535)");
            }

            tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), static_cast<unsigned short>(newPort));
            acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);

            boost::system::error_code ec;
            acceptor_->open(endpoint.protocol(), ec);
            if (ec)
            {
                spdlog::error("Failed to open acceptor socket: {} (Error code: {})", ec.message(), ec.value());
                throw std::system_error(ec, "Could not open the acceptor socket");
            }

            acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
            if (ec)
            {
                spdlog::error("Failed to set socket option: {} (Error code: {})", ec.message(), ec.value());
                throw std::system_error(ec, "Could not set socket option");
            }

            acceptor_->bind(endpoint, ec);
            if (ec)
            {
                spdlog::error("Failed to bind to the server port: {} (Error code: {})", ec.message(), ec.value());
                throw std::system_error(ec, "Could not bind to the server port");
            }

            acceptor_->listen(boost::asio::socket_base::max_connections, ec);
            if (ec)
            {
                spdlog::error("Failed to listen on the server port: {} (Error code: {})", ec.message(), ec.value());
                throw std::system_error(ec, "Could not listen on the server port");
            }
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error initializing server: {}", e.what());
            throw;
        }
    }

    HTTPServer::~HTTPServer() {}

    void HTTPServer::run()
    {
        try
        {
            route_configurator_->configure_routes();

            spdlog::info("Vix is running at http://127.0.0.1:{} using {} threads", config_.getServerPort(), NUMBER_OF_THREADS);
            spdlog::info("Waiting for incoming connections...");

            start_accept();

            std::size_t num_io_threads = calculate_io_thread_count();
            spdlog::info("Starting {} io_context threads", num_io_threads);

            for (std::size_t i = 0; i < num_io_threads; ++i)
            {
                io_threads_.emplace_back([this, i]()
                                         {
                    try
                    {
                        set_affinity(i); 
                        io_context_->run();
                    }
                    catch (const std::exception &e)
                    {
                        spdlog::error("Error in io_context (thread {}): {}", i, e.what());
                    }
                    spdlog::info("Thread {} finished.", i); });
            }

            for (auto &t : io_threads_)
            {
                if (t.joinable())
                    t.join();
            }
            spdlog::info("All io_context threads finished.");
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error in HTTPServer::run(): {}", e.what());
        }
    }

    int HTTPServer::calculate_io_thread_count()
    {
        return std::max(1, static_cast<int>(std::thread::hardware_concurrency() / 2));
    }

    void HTTPServer::start_accept()
    {
        auto socket = std::make_shared<tcp::socket>(*io_context_);

        try
        {
            acceptor_->async_accept(
                *socket,
                [this, socket](boost::system::error_code ec)
                {
                if (!ec)
                {
                    request_thread_pool_.enqueue(1, [this, socket]() {
                        try {
                            handle_client(socket, router_);
                        } catch (const std::exception &e) {
                            spdlog::error("Error handling client: {}", e.what());
                            close_socket(socket);
                        }
                    });
                }
                else
                {
                    spdlog::error("Error accepting connection from client: {} (Error code: {})", ec.message(), ec.value());
                }
                start_accept(); });
        }
        catch (const std::exception &e)
        {
            spdlog::error("Exception during async_accept: {}", e.what());
            acceptor_->close();
        }
    }

    void HTTPServer::close_socket(std::shared_ptr<tcp::socket> socket)
    {
        boost::system::error_code ec;
        socket->shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::system::error_code{})
        {
            spdlog::error("Failed to shutdown socket: {} (Error code: {})", ec.message(), ec.value());
        }
        socket->close(ec);
        if (ec && ec != boost::system::error_code{})
        {
            spdlog::error("Failed to close socket: {} (Error code: {})", ec.message(), ec.value());
        }
    }

    void HTTPServer::handle_client(std::shared_ptr<tcp::socket> socket_ptr, Router &router)
    {
        try
        {
            auto session = std::make_shared<Session>(std::move(*socket_ptr), router);
            session->run();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error in client session for client {}: {}", socket_ptr->remote_endpoint().address().to_string(), e.what());
            socket_ptr->close();
        }
    }

}
