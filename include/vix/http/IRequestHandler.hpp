/**
 * @file IRequestHandler.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_I_REQUEST_HANDLER_HPP
#define VIX_I_REQUEST_HANDLER_HPP

#include <vix/async/core/task.hpp>

namespace vix::http
{
  class Request;
  class Response;

  /**
   * @brief Interface for handling an HTTP request using the Vix async runtime.
   *
   * This interface is transport-agnostic and does not depend on Boost.Beast.
   * Implementations receive a Vix-native Request and fill a Vix-native Response.
   *
   * The handler is asynchronous and returns a Vix task, allowing the HTTP core
   * to await user logic without blocking the runtime.
   */
  class IRequestHandler
  {
  public:
    /**
     * @brief Handle an incoming request and populate the provided response.
     *
     * Implementations should inspect the request, write the response, and
     * `co_return` when complete.
     *
     * @param req Incoming Vix HTTP request.
     * @param res Outgoing Vix HTTP response to populate.
     * @return vix::async::core::task<void> asynchronous completion signal.
     */
    virtual vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) = 0;

    /**
     * @brief Construct a request handler.
     */
    IRequestHandler() noexcept = default;

    /**
     * @brief Destroy the request handler.
     */
    virtual ~IRequestHandler() noexcept = default;

    IRequestHandler(const IRequestHandler &) = delete;
    IRequestHandler &operator=(const IRequestHandler &) = delete;
    IRequestHandler(IRequestHandler &&) noexcept = delete;
    IRequestHandler &operator=(IRequestHandler &&) noexcept = delete;
  };

} // namespace vix::http

#endif // VIX_I_REQUEST_HANDLER_HPP
