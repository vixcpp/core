/**
 *
 * @file TlsConfig.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_TLS_CONFIG_HPP
#define VIX_TLS_CONFIG_HPP

#include <string>

namespace vix::server
{
  /**
   * @brief TLS configuration for the native Vix HTTP server.
   *
   * TLS is optional.
   *
   * When disabled, the server keeps accepting plain HTTP connections.
   * When enabled, the accepted TCP connection must complete a TLS handshake
   * before the HTTP session starts reading requests.
   */
  struct TlsConfig
  {
    /**
     * @brief Enable HTTPS/TLS for the HTTP server.
     */
    bool enabled{false};

    /**
     * @brief Path to the certificate file.
     *
     * In production this is usually the fullchain certificate file.
     *
     * Example:
     * /etc/letsencrypt/live/example.com/fullchain.pem
     */
    std::string cert_file{};

    /**
     * @brief Path to the private key file.
     *
     * Example:
     * /etc/letsencrypt/live/example.com/privkey.pem
     */
    std::string key_file{};

    /**
     * @brief Return true if TLS is enabled.
     */
    [[nodiscard]] bool is_enabled() const noexcept
    {
      return enabled;
    }

    /**
     * @brief Return true if TLS has enough configuration to start.
     *
     * This does not check whether the files exist.
     * It only validates that the required paths are not empty.
     */
    [[nodiscard]] bool is_configured() const noexcept
    {
      return !cert_file.empty() && !key_file.empty();
    }

    /**
     * @brief Return true if TLS can be used by the server.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return enabled && is_configured();
    }
  };

} // namespace vix::server

#endif // VIX_TLS_CONFIG_HPP
