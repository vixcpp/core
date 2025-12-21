#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include <vix/sync/NetworkProbe.hpp>
#include <vix/sync/outbox/Outbox.hpp>
#include <vix/sync/outbox/FileOutboxStore.hpp>
#include <vix/sync/engine/SyncEngine.hpp>

#include "fake_http_transport.hpp"

static std::int64_t now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void reset_test_dir(const std::filesystem::path &dir)
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
}

int main()
{
    using namespace vix::sync;
    using namespace vix::sync::outbox;
    using namespace vix::sync::engine;

    const std::filesystem::path test_dir = "./.vix_test_perm";
    reset_test_dir(test_dir);

    // 1) Outbox store
    auto store = std::make_shared<FileOutboxStore>(FileOutboxStore::Config{
        .file_path = test_dir / "outbox.json",
        .pretty_json = true,
        .fsync_on_write = false});

    auto outbox = std::make_shared<Outbox>(Outbox::Config{
                                               .owner = "test-engine",
                                           },
                                           store);

    // 2) Network probe: always online
    auto probe = std::make_shared<NetworkProbe>(
        NetworkProbe::Config{},
        []
        { return true; });

    // 3) Fake transport: permanent failure on this target
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->setRuleForTarget("/api/messages", FakeHttpTransport::Rule{
                                                     .ok = false,
                                                     .retryable = false,
                                                     .error = "bad request (permanent)"});

    // 4) Engine
    SyncEngine::Config ecfg;
    ecfg.worker_count = 1;
    ecfg.batch_limit = 10;
    ecfg.idle_sleep_ms = 0;
    ecfg.offline_sleep_ms = 0;

    SyncEngine engine(ecfg, outbox, probe, transport);

    // 5) Enqueue operation
    Operation op;
    op.kind = "http.post";
    op.target = "/api/messages";
    op.payload = R"({"text":"hello offline"})";

    const auto t0 = now_ms();
    const auto id = outbox->enqueue(op, t0);

    // 6) Tick engine => should attempt once and then mark PermanentFailed
    const auto processed1 = engine.tick(now_ms());
    assert(processed1 >= 1);
    assert(transport->callCount() == 1);

    auto saved1 = store->get(id);
    assert(saved1.has_value());
    assert(saved1->status == OperationStatus::PermanentFailed);
    assert(saved1->last_error.find("permanent") != std::string::npos);

    // 7) Tick again => MUST NOT retry (it should not come back from peek_ready)
    const auto processed2 = engine.tick(now_ms());
    // processed2 could be 0 (most likely) – but the key assertion is: no more sends
    (void)processed2;
    assert(transport->callCount() == 1);

    auto saved2 = store->get(id);
    assert(saved2.has_value());
    assert(saved2->status == OperationStatus::PermanentFailed);

    std::cout << "OK: permanent failure is not retried and status is PermanentFailed\n";
    return 0;
}
