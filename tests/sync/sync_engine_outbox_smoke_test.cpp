#include <cassert>
#include <chrono>
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

int main()
{
    using namespace vix::sync;
    using namespace vix::sync::outbox;
    using namespace vix::sync::engine;

    // 1) Outbox store
    auto store = std::make_shared<FileOutboxStore>(FileOutboxStore::Config{
        .file_path = "./.vix_test/outbox.json",
        .pretty_json = true,
        .fsync_on_write = false});

    auto outbox = std::make_shared<Outbox>(Outbox::Config{
                                               .owner = "test-engine",
                                           },
                                           store);

    // 2) Network probe: always online for this test
    auto probe = std::make_shared<NetworkProbe>(
        NetworkProbe::Config{},
        []
        { return true; });

    // 3) Fake transport: success
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->setDefault({.ok = true});

    // 4) Engine
    SyncEngine engine(SyncEngine::Config{
                          .worker_count = 1,
                          .batch_limit = 10},
                      outbox, probe, transport);

    // 5) Enqueue operation
    Operation op;
    op.kind = "http.post";
    op.target = "/api/messages";
    op.payload = R"({"text":"hello offline"})";

    const auto t0 = now_ms();
    const auto id = outbox->enqueue(op, t0);

    // 6) Tick engine => should send and complete
    const auto processed = engine.tick(now_ms());
    assert(processed >= 1);

    auto saved = store->get(id);
    assert(saved.has_value());
    assert(saved->status == OperationStatus::Done);

    std::cout << "OK: operation sent and marked Done\n";
    return 0;
}
