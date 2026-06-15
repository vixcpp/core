/**
 *
 * @file status_tests.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license thatcan be found in the License file.
 *
 * Vix.cpp
 *
 */
#include <cassert>
#include <string>
#include <string_view>

#include <vix/http/Status.hpp>

int main()
{
  using namespace vix::http;

  // ---------------------------------------------------------------------------
  // Constants: informational status codes
  // ---------------------------------------------------------------------------
  static_assert(CONTINUE == 100);
  static_assert(SWITCHING_PROTOCOLS == 101);
  static_assert(PROCESSING == 102);
  static_assert(EARLY_HINTS == 103);

  // ---------------------------------------------------------------------------
  // Constants: success status codes
  // ---------------------------------------------------------------------------
  static_assert(OK == 200);
  static_assert(CREATED == 201);
  static_assert(ACCEPTED == 202);
  static_assert(NON_AUTHORITATIVE_INFORMATION == 203);
  static_assert(NO_CONTENT == 204);
  static_assert(RESET_CONTENT == 205);
  static_assert(PARTIAL_CONTENT == 206);

  // ---------------------------------------------------------------------------
  // Constants: redirection status codes
  // ---------------------------------------------------------------------------
  static_assert(MULTIPLE_CHOICES == 300);
  static_assert(MOVED_PERMANENTLY == 301);
  static_assert(FOUND == 302);
  static_assert(SEE_OTHER == 303);
  static_assert(NOT_MODIFIED == 304);
  static_assert(TEMPORARY_REDIRECT == 307);
  static_assert(PERMANENT_REDIRECT == 308);

  // ---------------------------------------------------------------------------
  // Constants: client error status codes
  // ---------------------------------------------------------------------------
  static_assert(BAD_REQUEST == 400);
  static_assert(UNAUTHORIZED == 401);
  static_assert(PAYMENT_REQUIRED == 402);
  static_assert(FORBIDDEN == 403);
  static_assert(NOT_FOUND == 404);
  static_assert(METHOD_NOT_ALLOWED == 405);
  static_assert(NOT_ACCEPTABLE == 406);
  static_assert(PROXY_AUTHENTICATION_REQUIRED == 407);
  static_assert(REQUEST_TIMEOUT == 408);
  static_assert(CONFLICT == 409);
  static_assert(GONE == 410);
  static_assert(LENGTH_REQUIRED == 411);
  static_assert(PRECONDITION_FAILED == 412);
  static_assert(PAYLOAD_TOO_LARGE == 413);
  static_assert(URI_TOO_LONG == 414);
  static_assert(UNSUPPORTED_MEDIA_TYPE == 415);
  static_assert(RANGE_NOT_SATISFIABLE == 416);
  static_assert(EXPECTATION_FAILED == 417);
  static_assert(MISDIRECTED_REQUEST == 421);
  static_assert(UNPROCESSABLE_ENTITY == 422);
  static_assert(TOO_EARLY == 425);
  static_assert(UPGRADE_REQUIRED == 426);
  static_assert(TOO_MANY_REQUESTS == 429);

  // ---------------------------------------------------------------------------
  // Constants: server error status codes
  // ---------------------------------------------------------------------------
  static_assert(INTERNAL_ERROR == 500);
  static_assert(NOT_IMPLEMENTED == 501);
  static_assert(BAD_GATEWAY == 502);
  static_assert(SERVICE_UNAVAILABLE == 503);
  static_assert(GATEWAY_TIMEOUT == 504);
  static_assert(HTTP_VERSION_NOT_SUPPORTED == 505);

  // ---------------------------------------------------------------------------
  // Valid HTTP status range
  // ---------------------------------------------------------------------------
  static_assert(is_valid_status(100));
  static_assert(is_valid_status(101));
  static_assert(is_valid_status(199));
  static_assert(is_valid_status(200));
  static_assert(is_valid_status(299));
  static_assert(is_valid_status(300));
  static_assert(is_valid_status(399));
  static_assert(is_valid_status(400));
  static_assert(is_valid_status(499));
  static_assert(is_valid_status(500));
  static_assert(is_valid_status(599));

  static_assert(!is_valid_status(-1));
  static_assert(!is_valid_status(0));
  static_assert(!is_valid_status(99));
  static_assert(!is_valid_status(600));
  static_assert(!is_valid_status(999));

  // ---------------------------------------------------------------------------
  // normalize_status()
  // For valid status codes, it must return the original value.
  // Invalid status code behavior has an assert in debug builds, so we do not
  // runtime-test invalid values here.
  // ---------------------------------------------------------------------------
  static_assert(normalize_status(CONTINUE) == CONTINUE);
  static_assert(normalize_status(OK) == OK);
  static_assert(normalize_status(CREATED) == CREATED);
  static_assert(normalize_status(NO_CONTENT) == NO_CONTENT);
  static_assert(normalize_status(NOT_FOUND) == NOT_FOUND);
  static_assert(normalize_status(INTERNAL_ERROR) == INTERNAL_ERROR);
  static_assert(normalize_status(599) == 599);

  // ---------------------------------------------------------------------------
  // reason_phrase(): informational
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(CONTINUE) == std::string_view{"Continue"});
  static_assert(reason_phrase(SWITCHING_PROTOCOLS) == std::string_view{"Switching Protocols"});
  static_assert(reason_phrase(PROCESSING) == std::string_view{"Processing"});
  static_assert(reason_phrase(EARLY_HINTS) == std::string_view{"Early Hints"});

  // ---------------------------------------------------------------------------
  // reason_phrase(): success
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(OK) == std::string_view{"OK"});
  static_assert(reason_phrase(CREATED) == std::string_view{"Created"});
  static_assert(reason_phrase(ACCEPTED) == std::string_view{"Accepted"});
  static_assert(reason_phrase(NON_AUTHORITATIVE_INFORMATION) == std::string_view{"Non-Authoritative Information"});
  static_assert(reason_phrase(NO_CONTENT) == std::string_view{"No Content"});
  static_assert(reason_phrase(RESET_CONTENT) == std::string_view{"Reset Content"});
  static_assert(reason_phrase(PARTIAL_CONTENT) == std::string_view{"Partial Content"});

  // ---------------------------------------------------------------------------
  // reason_phrase(): redirection
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(MULTIPLE_CHOICES) == std::string_view{"Multiple Choices"});
  static_assert(reason_phrase(MOVED_PERMANENTLY) == std::string_view{"Moved Permanently"});
  static_assert(reason_phrase(FOUND) == std::string_view{"Found"});
  static_assert(reason_phrase(SEE_OTHER) == std::string_view{"See Other"});
  static_assert(reason_phrase(NOT_MODIFIED) == std::string_view{"Not Modified"});
  static_assert(reason_phrase(TEMPORARY_REDIRECT) == std::string_view{"Temporary Redirect"});
  static_assert(reason_phrase(PERMANENT_REDIRECT) == std::string_view{"Permanent Redirect"});

  // ---------------------------------------------------------------------------
  // reason_phrase(): client errors
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(BAD_REQUEST) == std::string_view{"Bad Request"});
  static_assert(reason_phrase(UNAUTHORIZED) == std::string_view{"Unauthorized"});
  static_assert(reason_phrase(PAYMENT_REQUIRED) == std::string_view{"Payment Required"});
  static_assert(reason_phrase(FORBIDDEN) == std::string_view{"Forbidden"});
  static_assert(reason_phrase(NOT_FOUND) == std::string_view{"Not Found"});
  static_assert(reason_phrase(METHOD_NOT_ALLOWED) == std::string_view{"Method Not Allowed"});
  static_assert(reason_phrase(NOT_ACCEPTABLE) == std::string_view{"Not Acceptable"});
  static_assert(reason_phrase(PROXY_AUTHENTICATION_REQUIRED) == std::string_view{"Proxy Authentication Required"});
  static_assert(reason_phrase(REQUEST_TIMEOUT) == std::string_view{"Request Timeout"});
  static_assert(reason_phrase(CONFLICT) == std::string_view{"Conflict"});
  static_assert(reason_phrase(GONE) == std::string_view{"Gone"});
  static_assert(reason_phrase(LENGTH_REQUIRED) == std::string_view{"Length Required"});
  static_assert(reason_phrase(PRECONDITION_FAILED) == std::string_view{"Precondition Failed"});
  static_assert(reason_phrase(PAYLOAD_TOO_LARGE) == std::string_view{"Payload Too Large"});
  static_assert(reason_phrase(URI_TOO_LONG) == std::string_view{"URI Too Long"});
  static_assert(reason_phrase(UNSUPPORTED_MEDIA_TYPE) == std::string_view{"Unsupported Media Type"});
  static_assert(reason_phrase(RANGE_NOT_SATISFIABLE) == std::string_view{"Range Not Satisfiable"});
  static_assert(reason_phrase(EXPECTATION_FAILED) == std::string_view{"Expectation Failed"});
  static_assert(reason_phrase(MISDIRECTED_REQUEST) == std::string_view{"Misdirected Request"});
  static_assert(reason_phrase(UNPROCESSABLE_ENTITY) == std::string_view{"Unprocessable Entity"});
  static_assert(reason_phrase(TOO_EARLY) == std::string_view{"Too Early"});
  static_assert(reason_phrase(UPGRADE_REQUIRED) == std::string_view{"Upgrade Required"});
  static_assert(reason_phrase(TOO_MANY_REQUESTS) == std::string_view{"Too Many Requests"});

  // ---------------------------------------------------------------------------
  // reason_phrase(): server errors
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(INTERNAL_ERROR) == std::string_view{"Internal Server Error"});
  static_assert(reason_phrase(NOT_IMPLEMENTED) == std::string_view{"Not Implemented"});
  static_assert(reason_phrase(BAD_GATEWAY) == std::string_view{"Bad Gateway"});
  static_assert(reason_phrase(SERVICE_UNAVAILABLE) == std::string_view{"Service Unavailable"});
  static_assert(reason_phrase(GATEWAY_TIMEOUT) == std::string_view{"Gateway Timeout"});
  static_assert(reason_phrase(HTTP_VERSION_NOT_SUPPORTED) == std::string_view{"HTTP Version Not Supported"});

  // ---------------------------------------------------------------------------
  // reason_phrase(): valid but unknown codes
  // ---------------------------------------------------------------------------
  static_assert(reason_phrase(199) == std::string_view{"Unknown"});
  static_assert(reason_phrase(299) == std::string_view{"Unknown"});
  static_assert(reason_phrase(399) == std::string_view{"Unknown"});
  static_assert(reason_phrase(499) == std::string_view{"Unknown"});
  static_assert(reason_phrase(599) == std::string_view{"Unknown"});

  // ---------------------------------------------------------------------------
  // status_to_string(): common known statuses
  // ---------------------------------------------------------------------------
  assert(status_to_string(CONTINUE) == "100 Continue");
  assert(status_to_string(SWITCHING_PROTOCOLS) == "101 Switching Protocols");
  assert(status_to_string(PROCESSING) == "102 Processing");
  assert(status_to_string(EARLY_HINTS) == "103 Early Hints");

  assert(status_to_string(OK) == "200 OK");
  assert(status_to_string(CREATED) == "201 Created");
  assert(status_to_string(ACCEPTED) == "202 Accepted");
  assert(status_to_string(NON_AUTHORITATIVE_INFORMATION) == "203 Non-Authoritative Information");
  assert(status_to_string(NO_CONTENT) == "204 No Content");
  assert(status_to_string(RESET_CONTENT) == "205 Reset Content");
  assert(status_to_string(PARTIAL_CONTENT) == "206 Partial Content");

  assert(status_to_string(MULTIPLE_CHOICES) == "300 Multiple Choices");
  assert(status_to_string(MOVED_PERMANENTLY) == "301 Moved Permanently");
  assert(status_to_string(FOUND) == "302 Found");
  assert(status_to_string(SEE_OTHER) == "303 See Other");
  assert(status_to_string(NOT_MODIFIED) == "304 Not Modified");
  assert(status_to_string(TEMPORARY_REDIRECT) == "307 Temporary Redirect");
  assert(status_to_string(PERMANENT_REDIRECT) == "308 Permanent Redirect");

  assert(status_to_string(BAD_REQUEST) == "400 Bad Request");
  assert(status_to_string(UNAUTHORIZED) == "401 Unauthorized");
  assert(status_to_string(PAYMENT_REQUIRED) == "402 Payment Required");
  assert(status_to_string(FORBIDDEN) == "403 Forbidden");
  assert(status_to_string(NOT_FOUND) == "404 Not Found");
  assert(status_to_string(METHOD_NOT_ALLOWED) == "405 Method Not Allowed");
  assert(status_to_string(NOT_ACCEPTABLE) == "406 Not Acceptable");
  assert(status_to_string(PROXY_AUTHENTICATION_REQUIRED) == "407 Proxy Authentication Required");
  assert(status_to_string(REQUEST_TIMEOUT) == "408 Request Timeout");
  assert(status_to_string(CONFLICT) == "409 Conflict");
  assert(status_to_string(GONE) == "410 Gone");
  assert(status_to_string(LENGTH_REQUIRED) == "411 Length Required");
  assert(status_to_string(PRECONDITION_FAILED) == "412 Precondition Failed");
  assert(status_to_string(PAYLOAD_TOO_LARGE) == "413 Payload Too Large");
  assert(status_to_string(URI_TOO_LONG) == "414 URI Too Long");
  assert(status_to_string(UNSUPPORTED_MEDIA_TYPE) == "415 Unsupported Media Type");
  assert(status_to_string(RANGE_NOT_SATISFIABLE) == "416 Range Not Satisfiable");
  assert(status_to_string(EXPECTATION_FAILED) == "417 Expectation Failed");
  assert(status_to_string(MISDIRECTED_REQUEST) == "421 Misdirected Request");
  assert(status_to_string(UNPROCESSABLE_ENTITY) == "422 Unprocessable Entity");
  assert(status_to_string(TOO_EARLY) == "425 Too Early");
  assert(status_to_string(UPGRADE_REQUIRED) == "426 Upgrade Required");
  assert(status_to_string(TOO_MANY_REQUESTS) == "429 Too Many Requests");

  assert(status_to_string(INTERNAL_ERROR) == "500 Internal Server Error");
  assert(status_to_string(NOT_IMPLEMENTED) == "501 Not Implemented");
  assert(status_to_string(BAD_GATEWAY) == "502 Bad Gateway");
  assert(status_to_string(SERVICE_UNAVAILABLE) == "503 Service Unavailable");
  assert(status_to_string(GATEWAY_TIMEOUT) == "504 Gateway Timeout");
  assert(status_to_string(HTTP_VERSION_NOT_SUPPORTED) == "505 HTTP Version Not Supported");

  // ---------------------------------------------------------------------------
  // status_to_string(): valid but unknown status codes.
  // These must stay valid and produce "Unknown", not crash.
  // ---------------------------------------------------------------------------
  assert(status_to_string(199) == "199 Unknown");
  assert(status_to_string(299) == "299 Unknown");
  assert(status_to_string(399) == "399 Unknown");
  assert(status_to_string(499) == "499 Unknown");
  assert(status_to_string(599) == "599 Unknown");

  return 0;
}
