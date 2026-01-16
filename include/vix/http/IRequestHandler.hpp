/**
 *
 *  @file IRequestHandler.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_I_REQUEST_HANDLER_HPP
#define VIX_I_REQUEST_HANDLER_HPP

#include <boost/beast/http.hpp>

namespace vix::vhttp
{

  namespace http = boost::beast::http;

  class IRequestHandler
  {
  public:
    virtual void handle_request(
        const http::request<http::string_body> &req,
        http::response<http::string_body> &res) = 0;

    IRequestHandler() noexcept = default;
    virtual ~IRequestHandler() noexcept = default;
    IRequestHandler(const IRequestHandler &) = delete;
    IRequestHandler &operator=(const IRequestHandler &) = delete;
    IRequestHandler(IRequestHandler &&) noexcept = delete;
    IRequestHandler &operator=(IRequestHandler &&) noexcept = delete;
  };

} // namespace vix::http

#endif // VIX_I_REQUEST_HANDLER_HPP
