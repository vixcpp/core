#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <optional>

#include <vix/sync/NetworkProbe.hpp>

#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/CacheContext.hpp>
#include <vix/http/cache/CacheContextMapper.hpp>
#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/MemoryStore.hpp>

static std::int64_t now_ms()
{
    using namespace std::chrono;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

enum class Decision
{
    CacheHit,
    NetOk,
    OfflineMiss,
    ErrorMiss
};

struct NetResult
{
    bool ok{false}; // request succeeded (reached server)
    bool network_error{false};
    int status{0};
    std::string body;
};

// Pure function we can test
static Decision handle_get_with_cache(vix::vhttp::cache::Cache &cache,
                                      const std::string &key,
                                      std::int64_t t,
                                      vix::sync::NetworkProbe &probe,
                                      const NetResult &net,
                                      std::optional<vix::vhttp::cache::CacheContext> forced_ctx = std::nullopt)
{
    using namespace vix::vhttp::cache;

    // 1) Determine context (test override if provided)
    CacheContext ctx = forced_ctx ? *forced_ctx : contextFromProbe(probe, t);

    // 2) Offline => cache only
    if (ctx.offline)
    {
        auto cached = cache.get(key, t, ctx);
        return cached ? Decision::CacheHit : Decision::OfflineMiss;
    }

    // 3) Online => attempt network
    if (net.ok)
    {
        CacheEntry e;
        e.status = net.status;
        e.body = net.body;
        e.created_at_ms = t;
        cache.put(key, e);
        return Decision::NetOk;
    }

    // 4) Network error => fallback cache
    if (net.network_error)
    {
        CacheContext err_ctx = forced_ctx ? *forced_ctx
                                          : contextFromProbeAndOutcome(probe, t, RequestOutcome::NetworkError);
        // Important: we want "network_error=true" in this context
        err_ctx.network_error = true;

        auto cached = cache.get(key, t, err_ctx);
        return cached ? Decision::CacheHit : Decision::ErrorMiss;
    }

    return Decision::ErrorMiss;
}

static void test_offline_cache_hit()
{
    using namespace vix::vhttp::cache;

    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 100;
    policy.stale_if_offline_ms = 10'000;
    policy.allow_stale_if_offline = true;

    Cache cache(policy, store);

    const std::string key = "GET:/api/users?page=1";
    const auto t0 = now_ms();

    CacheEntry e;
    e.status = 200;
    e.body = R"({"cached":true})";
    e.created_at_ms = t0;
    cache.put(key, e);

    vix::sync::NetworkProbe probe(
        vix::sync::NetworkProbe::Config{},
        []
        { return false; } // offline
    );

    NetResult net{};
    auto d = handle_get_with_cache(cache, key, t0 + 5000, probe, net);
    assert(d == Decision::CacheHit);

    std::cout << "[OK] offline -> cache hit\n";
}

static void test_offline_cache_miss()
{
    using namespace vix::vhttp::cache;

    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 100;
    policy.stale_if_offline_ms = 10'000;
    policy.allow_stale_if_offline = true;

    Cache cache(policy, store);

    const std::string key = "GET:/api/missing";
    const auto t0 = now_ms();

    vix::sync::NetworkProbe probe(
        vix::sync::NetworkProbe::Config{},
        []
        { return false; } // offline
    );

    NetResult net{};
    auto d = handle_get_with_cache(cache, key, t0, probe, net);
    assert(d == Decision::OfflineMiss);

    std::cout << "[OK] offline -> cache miss\n";
}

static void test_online_network_ok_populates_cache()
{
    using namespace vix::vhttp::cache;

    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 10'000;
    Cache cache(policy, store);

    const std::string key = "GET:/api/products?limit=10";
    const auto t0 = now_ms();

    vix::sync::NetworkProbe probe(
        vix::sync::NetworkProbe::Config{},
        []
        { return true; } // online
    );

    NetResult net{};
    net.ok = true;
    net.status = 200;
    net.body = R"({"from":"network"})";

    auto d = handle_get_with_cache(cache, key, t0, probe, net, CacheContext::Online());
    assert(d == Decision::NetOk);

    // Now ensure cached
    auto got = cache.get(key, t0 + 1, CacheContext::Online());
    assert(got.has_value());
    assert(got->body == R"({"from":"network"})");

    std::cout << "[OK] online + net ok -> cache populated\n";
}

static void test_online_network_error_fallback_cache()
{
    using namespace vix::vhttp::cache;

    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 100;
    policy.allow_stale_if_error = true;
    policy.stale_if_error_ms = 10'000;

    Cache cache(policy, store);

    const std::string key = "GET:/api/profile";
    const auto t0 = now_ms();

    // Put an old entry
    CacheEntry e;
    e.status = 200;
    e.body = R"({"cached":"profile"})";
    e.created_at_ms = t0;
    cache.put(key, e);

    vix::sync::NetworkProbe probe(
        vix::sync::NetworkProbe::Config{},
        []
        { return true; } // probe says online
    );

    NetResult net{};
    net.ok = false;
    net.network_error = true;

    // After 4000ms: entry is expired (ttl=100) but within stale_if_error window (10s)
    auto d = handle_get_with_cache(cache, key, t0 + 4000, probe, net, CacheContext::NetworkError());
    assert(d == Decision::CacheHit);

    std::cout << "[OK] online + network error -> stale-if-error cache hit\n";
}

int main()
{
    test_offline_cache_hit();
    test_offline_cache_miss();
    test_online_network_ok_populates_cache();
    test_online_network_error_fallback_cache();

    std::cout << "OK: cache context mapper smoke tests passed\n";
    return 0;
}
