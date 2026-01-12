#include <vix/core.h>
#include <cassert>

int main()
{
    vix::http::response<vix::http::string_body> raw;

    vix::Response res(raw);

    res.status(200);
    res.send("ok");

    assert(raw.result_int() == 200);
    assert(!raw.body().empty());

    return 0;
}
