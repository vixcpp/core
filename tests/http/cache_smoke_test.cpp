#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/MemoryStore.hpp>
#include <vix/http/cache/FileStore.hpp>

static std::int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void test_memory_store()
{
    using namespace vix::http::cache;

    auto store = std::make_shared<MemoryStore>();

    CachePolicy policy;
    policy.ttl_ms = 100;                // 100ms fresh
    policy.stale_if_offline_ms = 1'000; // 1s stale allowed if offline
    policy.allow_stale_if_offline = true;

    // ✅ Important: disable stale-if-error for this smoke test.
    // In the current Cache::get API, network_ok=false can also be interpreted
    // as "network error", so stale_if_error could keep returning a value.
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
        auto got = cache.get(key, t0 + 50, /*network_ok=*/true);
        assert(got.has_value());
        assert(got->status == 200);
        assert(got->body == R"({"ok":true})");
    }

    // 2) Stale but offline allowed
    {
        auto got = cache.get(key, t0 + 500, /*network_ok=*/false);
        assert(got.has_value());
        assert(got->body == R"({"ok":true})");
    }

    // 3) Too old => reject even if offline (beyond stale_if_offline_ms)
    {
        auto got = cache.get(key, t0 + 5000, /*network_ok=*/false);
        assert(!got.has_value());
    }

    std::cout << "[OK] MemoryStore cache smoke\n";
}

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

    // ✅ Same reasoning as above
    policy.allow_stale_if_error = false;
    policy.stale_if_error_ms = 0;

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
        auto got = cache2.get(key, t0 + 50, /*network_ok=*/true);
        assert(got.has_value());
        assert(got->body == R"({"items":[1,2,3]})");
    }

    // 2) Stale hit if offline after reload
    {
        auto got = cache2.get(key, t0 + 1000, /*network_ok=*/false);
        assert(got.has_value());
    }

    // Cleanup (optional)
    // std::filesystem::remove(file);

    std::cout << "[OK] FileStore cache smoke\n";
}

int main()
{
    test_memory_store();
    test_file_store();
    std::cout << "OK: http cache smoke tests passed\n";
    return 0;
}
