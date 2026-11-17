#pragma once

#include <functional>
#include <list>
#include <utility>

class CTimer;

class Timers
{
public:
    using Callback = std::function<float()>;
    using Create = std::function<CTimer*(float, Callback)>;
    using Remove = std::function<void(CTimer*)>;

    void Add(float delay, Callback callback, const Create& create)
    {
        auto item = m_pending.insert(m_pending.end(), nullptr);
        auto* timer = create(delay, [this, item, callback = std::move(callback)]() {
            float next = callback();
            if (next < 0.0f)
                m_pending.erase(item);
            return next;
        });
        if (timer)
            *item = timer;
        else
            m_pending.erase(item);
    }

    void Clear(const Remove& remove)
    {
        for (auto* timer : m_pending)
            remove(timer);
        m_pending.clear();
    }

    void Forget()
    {
        m_pending.clear();
    }

private:
    std::list<CTimer*> m_pending;
};
