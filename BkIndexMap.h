#include <map>
#include <atomic>
#include <vector>
#include <thread>
#include <condition_variable>
#include <iostream>

/********************************************************************************************************
 * Not production code
 * 
 * BkIdxMap : A wrapper class for a map. The idea is to combine the flexibility of a map, with the speed of
 * a vector. This is acheived by using a another thread to build a vector-index into the map, using background processing
 * This in many setups is "free", as few existing applications use multi-threading effectively. It will however use more
 * memory, and the syncronisation (via an atomic), is not free.
 * If the index is not built yet, the native map find is used.
 * 
 * threading
 * ---------
 * In a real-world system, allocating a thread per map-instance is impractical, and the thread loop should be converted
 * to a "task", to be added to a task-pool (std::async for example could be used to do this).
 * A quick  fix, might be to make the thread static (one thread for all instances)
 * 
 * states
 * ------
 * The model used is a "mater/slave" relationship, where the background thread is only capable of changing
 * mutated->indexing
 * indexing->stable
 * Using a compare/exchange to enforce this, means that if the master thread changes the state when the background thread
 * is busy, we never over-write the change because of a background thread event.
 * This should mean the background thread will eventually do the right thing. (and eventual consistancy is all we promise)
 * 
 * memory
 * ======
 * We use the default model of expanding vector storage only. This means that the system quickly hits a state where no 
 * allocations are happening, but it does mean that we almost always allocate more memory than required.
 * It would be trivial to change the system to allow "shrink to fit" operations on the vector
 * 
 * Locking
 * =======
 * You may need to perform a block of operations on the structure, and wish to avoid the overhead of repeatedly locking/unlocking.
 * Locking is cheap (it's an atomic) but cheap is not free. 
 * A PerformLocked is structured way to pass a arbitrary function to be performed whilst locked. While this looks messy, it has
 * the advantage of being impossible to break.
 * Recursive locks/unlocks are allowed.(so you can for example call member functions from PerformLocked)
 * Whilst locked the background thread will idle.
 * 
 * 
 * ToDo
 * ----
 * Add a second layer of indexing, time to see the speed impact. (Done: goes from 3* to4* faster)
 * Add more map functions (range-insert would be a good one for example)
********************************************************************************************************/


class BkIdxMapBase
{
protected:
    enum class State
    {
        Stable,   // indexing is complete (or aborted)
        MutateBegin, // we are waiting for the bk thread to abort
        Mutating, // we are waiting for the bk thread to abort
        Mutated,  // index requires rebuilding
        Indexing1,  // indexis being built
        Indexing2,  // a second layer of sampled key values is being built
        Quit,
        QuitDone
    };
    std::atomic<State> m_state{State::Stable};

    // abstract operations, in case more complex (say a mutex) based solution is required later.
    // these are "publuc facing" functions for the main thread. Abstracted via LockRAII
    void Lock();
    void Unlock();

    // this is the background thread equivelent of the lock.
    struct LockPrivate
    {
        BkIdxMapBase &base;
        std::unique_lock<std::mutex> lg;
        LockPrivate(BkIdxMapBase &b)
            : base(b)
        {
        }
        ~LockPrivate()
        {
        }
        void Wait() // wait for mutation
        {
            while (base.m_state==State::Stable)
                /*Sleep?*/;
        }
        void Release()
        {
        }
    };

public:
    struct LockRAII
    {
        BkIdxMapBase &base;
        LockRAII(BkIdxMapBase &b)
            : base(b)
        {
            base.Lock();
        }
        ~LockRAII()
        {
            base.Unlock();
        }
    };
    bool IsFastIndexAvailable() const { return m_state == State::Stable; }
};

template <class K, class V>
class BkIdxMap : BkIdxMapBase
{
    using Impl = std::map<K, V>;
    using Iterator = typename Impl::iterator;
    Impl m_impl;
    static constexpr int step = 256;
    std::vector<K> m_index_level1; // paired with m_index_iter, holds the k for each iterator 
    std::vector<Iterator> m_index_iter;
    std::vector<K> m_index_level2; // this is a list of sampled values at "step" intervals
    std::thread m_idxThread;

public:
    BkIdxMap();
    ~BkIdxMap();
    //PerformLocked assumes the structure is mutated
    template <class Fn>
    void PerformLocked(Fn &&fn) {BkIdxMapBase::LockRAII lock(*this); fn();}

    inline void Add(const K &, const V &);
    inline void Remove(const K &);
    inline Iterator Find(const K &) noexcept;
    inline bool IsFastIndexAvailable() const { return m_state == State::Stable; }

    Iterator End() { return m_impl.end(); }
    Iterator Begin() { return m_impl.begin(); }
    Iterator End() const { return m_impl.end(); }
    Iterator Begin() const { return m_impl.begin(); }

    bool Empty() const noexcept { return m_impl.empty(); }
    std::size_t Size() const noexcept { return m_impl.size(); }
    void Clear() noexcept { BkIdxMapBase::LockRAII lock(*this); m_impl.clear(); }

    // for timing only, cannot be undone
    void DisableIndexing() { m_state = State::Quit; m_idxThread.join(); }
};

template <class K, class V>
BkIdxMap<K,V>::BkIdxMap()
{
    m_idxThread = std::thread([this](){
        Iterator progress;
        int lvl2_index{0};
        while(true)
        {
            BkIdxMapBase::LockPrivate priv_lock(*this);
            State expected;
            switch(m_state)
            {
                // respond to signals, the main thread is in a busy wait state, so no exchange required
                case State::Quit:
                    m_state = State::QuitDone;
                    return;
                case State::MutateBegin:
                    progress = m_impl.end();
                    m_state = State::Stable;
                    break;
                case State::QuitDone:
                    /*NEVER HANDLED*/;
                    break;
                // states that mean we are waiting on the other thread to do something
                case State::Mutating:
                    priv_lock.Wait();
                    break;
                case State::Stable:
                    priv_lock.Wait();
                    break;
                // processing states. If other thread forces a state change, respect thier change (compare/exchange)
                case State::Mutated:
                    m_index_level1.clear();
                    m_index_iter.clear();
                    progress = m_impl.begin();
                    expected = State::Mutated;
                    m_state.compare_exchange_strong(expected, State::Indexing1);
                    break;
                case State::Indexing1:
                    m_index_level1.emplace_back(progress->first);
                    m_index_iter.emplace_back(progress);
                    progress++;
                    if (progress==m_impl.end())
                    {
                        expected = State::Indexing1;
                        lvl2_index = 0;
                        m_index_level2.clear();
                        m_state.compare_exchange_strong(expected, State::Indexing2);
                    }
                    break;
                case State::Indexing2:
                {
                    if ((size_t)lvl2_index >= m_index_level1.size())
                    {
                        expected = State::Indexing2;
                        m_state.compare_exchange_strong(expected, State::Stable);
                    }
                    else 
                    {
                        m_index_level2.emplace_back(m_index_level1[lvl2_index]);
                        lvl2_index += step;
                    }
                    break;
                }
            }
        }
    });
}

template <class K, class V>
inline BkIdxMap<K,V>::~BkIdxMap()
{
    if (m_idxThread.joinable())
    {
        m_state = State::Quit;
        while (m_state!=State::QuitDone)
            /*Wait*/;
        m_idxThread.join();
    }
}

template <class K, class V>
inline void BkIdxMap<K,V>::Add(const K& k, const V& v)
{
    LockRAII lock(*this);
    m_impl.insert( std::make_pair( k, v) );
}

template <class K, class V>
inline void BkIdxMap<K,V>::Remove(const K& k)
{
    LockRAII lock(*this);
    m_impl.erase( k );
}

template <class K, class V>
inline typename BkIdxMap<K,V>::Iterator BkIdxMap<K,V>::Find(const K& k) noexcept
{
    if (IsFastIndexAvailable())
    {
        const auto found2  = std::lower_bound(m_index_level2.begin(), m_index_level2.end(), k);
        const auto dist_start = std::max(0,(int)(found2-m_index_level2.begin())-1);
        const auto section_start = m_index_level1.begin()+(step*dist_start);
        const auto max_size = std::min(m_index_level1.size()-(section_start-m_index_level1.begin()), (std::size_t)step);
        const auto section_end = section_start + max_size;
        const auto found = std::lower_bound(section_start, section_end, k);
        if (found == m_index_level1.end() || (*found!=k))
            return m_impl.end();
        return m_index_iter[found-m_index_level1.begin()];
    }
    else
        return m_impl.find(k);
}
