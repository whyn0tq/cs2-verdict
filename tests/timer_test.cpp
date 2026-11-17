#include "timers.h"

#include <cassert>
#include <memory>

class CTimer
{
public:
    Timers::Callback run;
};

int main()
{
    Timers timers;
    int calls = 0;
    auto create = [](float delay, Timers::Callback callback) {
        assert(delay == 1.0f);
        return new CTimer{std::move(callback)};
    };

    CTimer* finished = nullptr;
    timers.Add(1.0f, [&calls]() {
        ++calls;
        return -1.0f;
    }, [&](float delay, Timers::Callback callback) {
        return finished = create(delay, std::move(callback));
    });
    assert(finished->run() < 0.0f);
    delete finished;

    CTimer* pending = nullptr;
    auto data = std::make_shared<int>(42);
    std::weak_ptr<int> payload = data;
    timers.Add(1.0f, [data, &calls]() {
        assert(*data == 42);
        ++calls;
        return 1.0f;
    }, [&](float delay, Timers::Callback callback) {
        return pending = create(delay, std::move(callback));
    });
    data.reset();
    assert(pending->run() == 1.0f);

    auto remove = [&](CTimer* timer) {
        assert(timer == pending);
        delete timer;
        pending = nullptr;
    };
    timers.Clear(remove);
    timers.Clear(remove);
    assert(!pending && payload.expired() && calls == 2);
}
