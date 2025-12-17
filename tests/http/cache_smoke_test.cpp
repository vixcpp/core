#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/CacheContext.hpp>
#include <vix/http/cache/CacheEntry.hpp>
#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/MemoryStore.hpp>
#include <vix/http/cache/FileStore.hpp>

#include <vix/http/cache/LruMemoryStore.hpp>
#include <vix/http/cache/CacheKey.hpp>

static std::int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// A) Test existant : MemoryStore + Policy
static void test_memory_store()
{
    using namespace vix::http::cache;

    auto store = std::make_shared<MemoryStore>();

    CachePolicy policy;
    policy.ttl_ms = 100;                // 100ms fresh
    policy.stale_if_offline_ms = 1'000; // 1s stale allowed if offline
    policy.allow_stale_if_offline = true;

    // ⛔ stale-if-error disabled initially
    policy.allow_stale_if_error = false;
    policy.stale_if_error_ms = 0;

    Cache cache(policy, store);

    const std::string key = "GET:/api/users?page=1";
    const auto t0 = now_ms();

    CacheEntry e;
    e.status = 200;
    e.body = R"({"ok":true})";
    e.created_at_ms = t0;

    cache.put(key, e);

    // 1) Fresh hit
    {
        auto got = cache.get(key, t0 + 50, CacheContext::Online());
        assert(got.has_value());
        assert(got->status == 200);
        assert(got->body == R"({"ok":true})");
    }

    // 2) Stale but offline allowed
    {
        auto got = cache.get(key, t0 + 500, CacheContext::Offline());
        assert(got.has_value());
        assert(got->body == R"({"ok":true})");
    }

    // 3) Too old => reject even if offline
    {
        auto got = cache.get(key, t0 + 5000, CacheContext::Offline());
        assert(!got.has_value());
    }

    // 4) Network error → stale-if-error allowed
    policy.allow_stale_if_error = true;
    policy.stale_if_error_ms = 5'000;

    Cache cache_with_error_policy(policy, store);

    {
        auto got = cache_with_error_policy.get(
            key,
            t0 + 4000, // expired but within stale_if_error window
            CacheContext::NetworkError());
        assert(got.has_value());
        assert(got->body == R"({"ok":true})");
    }

    std::cout << "[OK] MemoryStore cache smoke (offline + network_error)\n";
}

// B) Test existant : FileStore persistence + reload
static void test_file_store()
{
    using namespace vix::http::cache;

    const std::filesystem::path dir = "./build/.vix_test";
    const std::filesystem::path file = dir / "cache_http.json";

    std::filesystem::create_directories(dir);
    if (std::filesystem::exists(file))
        std::filesystem::remove(file);

    auto store = std::make_shared<FileStore>(FileStore::Config{
        .file_path = file,
        .pretty_json = true});

    CachePolicy policy;
    policy.ttl_ms = 100;
    policy.stale_if_offline_ms = 2'000;
    policy.allow_stale_if_offline = true;

    Cache cache(policy, store);

    const std::string key = "GET:/api/products?limit=10";
    const auto t0 = now_ms();

    CacheEntry e;
    e.status = 200;
    e.body = R"({"items":[1,2,3]})";
    e.created_at_ms = t0;

    cache.put(key, e);

    // Recreate store+cache to ensure persistence works (reload from disk)
    auto store2 = std::make_shared<FileStore>(FileStore::Config{
        .file_path = file,
        .pretty_json = false});
    Cache cache2(policy, store2);

    // 1) Fresh hit after reload
    {
        auto got = cache2.get(key, t0 + 50, CacheContext::Online());
        assert(got.has_value());
        assert(got->body == R"({"items":[1,2,3]})");
    }

    // 2) Stale hit if offline after reload
    {
        auto got = cache2.get(key, t0 + 1000, CacheContext::Offline());
        assert(got.has_value());
    }

    std::cout << "[OK] FileStore cache smoke\n";
}

// C) Cache::put() normalise les headers en lower-case
static void test_header_normalization_on_put()
{
    using namespace vix::http::cache;

    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 10'000;

    Cache cache(policy, store);

    const auto t0 = now_ms();
    CacheEntry e;
    e.status = 200;
    e.body = "x";
    e.created_at_ms = t0;

    // Mixed-case keys
    e.headers["Content-Type"] = "application/json";
    e.headers["X-Powered-By"] = "Vix";

    cache.put("k", e);

    auto got = cache.get("k", t0 + 1, CacheContext::Online());
    assert(got.has_value());

    // keys must be lower-case now
    assert(got->headers.find("content-type") != got->headers.end());
    assert(got->headers.find("x-powered-by") != got->headers.end());

    // and old keys should typically not exist (last-wins normalization)
    assert(got->headers.find("Content-Type") == got->headers.end());

    std::cout << "[OK] Header normalization on Cache::put\n";
}

// D) LruMemoryStore eviction (max_entries)
static void test_lru_eviction()
{
    using namespace vix::http::cache;

    auto store = std::make_shared<LruMemoryStore>(LruMemoryStore::Config{.max_entries = 2});
    CachePolicy policy;
    policy.ttl_ms = 10'000;

    Cache cache(policy, store);

    const auto t0 = now_ms();

    CacheEntry e1{200, "A", {}, t0};
    CacheEntry e2{200, "B", {}, t0};
    CacheEntry e3{200, "C", {}, t0};

    cache.put("k1", e1);
    cache.put("k2", e2);

    // touch k1 => k2 becomes LRU
    (void)cache.get("k1", t0 + 1, CacheContext::Online());

    // put k3 => should evict k2
    cache.put("k3", e3);

    auto g1 = cache.get("k1", t0 + 2, CacheContext::Online());
    auto g2 = cache.get("k2", t0 + 2, CacheContext::Online());
    auto g3 = cache.get("k3", t0 + 2, CacheContext::Online());

    assert(g1.has_value());
    assert(!g2.has_value());
    assert(g3.has_value());

    std::cout << "[OK] LruMemoryStore eviction (max_entries)\n";
}

// E) Cache::prune() supprime les entrées trop vieilles (LRU store)
static void test_prune_on_lru_store()
{
    using namespace vix::http::cache;

    // temps "fake" stable
    const std::int64_t t0 = 1'000'000;

    auto store = std::make_shared<LruMemoryStore>(LruMemoryStore::Config{.max_entries = 1024});

    CachePolicy policy;
    policy.ttl_ms = 1'000;                 // 1s
    policy.allow_stale_if_offline = false; // max_age == ttl
    policy.allow_stale_if_error = false;

    Cache cache(policy, store);

    // stale (trop vieux) => doit partir
    CacheEntry stale;
    stale.status = 200;
    stale.body = "stale";
    stale.created_at_ms = t0 - 5'000; // age = 5000ms > ttl
    cache.put("k_stale", stale);

    // fresh => doit rester
    CacheEntry fresh;
    fresh.status = 200;
    fresh.body = "fresh";
    fresh.created_at_ms = t0; // age = 0
    cache.put("k_fresh", fresh);

    //  prune AVANT expiration du fresh (age = 900ms < 1000ms)
    const std::int64_t t_prune = t0 + 900;
    const auto removed = cache.prune(t_prune);
    (void)removed;

    // Vérifie "présence en stockage"
    auto s_stale = store->get("k_stale");
    auto s_fresh = store->get("k_fresh");
    assert(!s_stale.has_value());
    assert(s_fresh.has_value());

    // Vérifie aussi via cache.get (fresh encore valide)
    auto g_fresh = cache.get("k_fresh", t0 + 900, CacheContext::Online());
    assert(g_fresh.has_value());

    std::cout << "[OK] prune() on LruMemoryStore removes stale, keeps fresh\n";
}

// F) CacheKey normalise query + support include_headers
static void test_cache_key_builder()
{
    using namespace vix::http::cache;

    std::unordered_map<std::string, std::string> headers;
    headers["Accept"] = "application/json";
    headers["X-Device"] = "mobile";

    // query out-of-order => must normalize to a=1&b=2
    const auto k1 = CacheKey::fromRequest(
        "GET",
        "/api/users",
        "b=2&a=1",
        headers);

    assert(k1.find("GET /api/users?a=1&b=2") != std::string::npos);

    // include_headers => key varies on selected header(s)
    const auto k2 = CacheKey::fromRequest(
        "GET",
        "/api/users",
        "b=2&a=1",
        headers,
        {"Accept"});

    assert(k2.find("|h:accept=application/json;") != std::string::npos);

    std::cout << "[OK] CacheKey builder (query normalize + vary headers)\n";
}

int main()
{
    test_memory_store();
    test_file_store();

    test_header_normalization_on_put();
    test_lru_eviction();
    test_prune_on_lru_store();
    test_cache_key_builder();

    std::cout << "OK: http cache smoke tests passed\n";
    return 0;
}
