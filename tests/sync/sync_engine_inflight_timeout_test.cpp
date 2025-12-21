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

    const std::filesystem::path test_dir = "./.vix_test_inflight";
    reset_test_dir(test_dir);

    // 1) Store
    auto store = std::make_shared<FileOutboxStore>(FileOutboxStore::Config{
        .file_path = test_dir / "outbox.json",
        .pretty_json = true,
        .fsync_on_write = false});

    auto outbox = std::make_shared<Outbox>(Outbox::Config{
                                               .owner = "test-engine",
                                           },
                                           store);

    // 2) Probe: always online
    auto probe = std::make_shared<NetworkProbe>(
        NetworkProbe::Config{},
        []
        { return true; });

    // 3) Transport: success
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->setDefault({.ok = true});

    // 4) Engine with short inflight timeout
    SyncEngine::Config ecfg;
    ecfg.worker_count = 1;
    ecfg.batch_limit = 10;
    ecfg.idle_sleep_ms = 0;
    ecfg.offline_sleep_ms = 0;
    ecfg.inflight_timeout_ms = 50; // 50ms for test

    SyncEngine engine(ecfg, outbox, probe, transport);

    // 5) Enqueue op
    Operation op;
    op.kind = "http.post";
    op.target = "/api/messages";
    op.payload = R"({"text":"hello offline"})";

    const auto t0 = now_ms();
    const auto id = outbox->enqueue(op, t0);

    // 6) Simulate crash after claim: claim manually and do NOT complete/fail
    const bool claimed = outbox->claim(id, t0);
    assert(claimed);

    {
        auto saved = store->get(id);
        assert(saved.has_value());
        assert(saved->status == OperationStatus::InFlight);
    }

    // 7) Advance beyond timeout => sweep should requeue (InFlight -> Failed)
    const auto t1 = t0 + 60; // > 50ms timeout
    engine.tick(t1);

    {
        auto saved = store->get(id);
        assert(saved.has_value());
        // after sweep it must no longer be stuck InFlight
        assert(saved->status != OperationStatus::InFlight);
        // typical expected status right after sweep:
        assert(saved->status == OperationStatus::Failed || saved->status == OperationStatus::Done);
    }

    // 8) Tick again => should resend and complete
    engine.tick(t1 + 1);

    auto final = store->get(id);
    assert(final.has_value());
    assert(final->status == OperationStatus::Done);

    // Must have at least one send attempt (after requeue)
    assert(transport->callCount() >= 1);

    std::cout << "OK: inflight timeout requeues stuck ops and they complete\n";
    return 0;
}
