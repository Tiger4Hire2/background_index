#include "BkIndexMap.h"
#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

void BkIdxMapBase::Lock()
{
    // indicate you want to mutate and wait for the reply from the background thread
    if (m_state == State::Mutating)
    {
        m_state = State::MutateBegin;
        while(m_state!=State::Stable)
            /*Wait*/;
        m_state = State::Mutating;
    }
}

void BkIdxMapBase::Unlock()
{
    m_state = State::Mutated;
}

TEST(IndexedMap, Construct)
{
    BkIdxMap<int,int> simple_map;
}

TEST(IndexedMap, Initailise)
{
    BkIdxMap<int,int> simple_map;
    for (int i = 0; i < 1000; ++i)
        simple_map.Add(i, i+1);
}

TEST(IndexedMap, InitailiseAndWait)
{
    BkIdxMap<int,int> simple_map;
    for (int i = 0; i < 1000; ++i)
        simple_map.Add(i, i+1);
    while (!simple_map.IsFastIndexAvailable())
       /*wait*/;
}


TEST(IndexedMap, Reindex)
{
    BkIdxMap<int,int> simple_map;
    for (int i = 0; i < 1000; ++i)
        simple_map.Add(i, i+1);
    EXPECT_FALSE(simple_map.IsFastIndexAvailable());
    // add more
    for (int i = 1000; i < 1010; ++i)
        simple_map.Add(i, i+1);
    //but still finish
    EXPECT_FALSE(simple_map.IsFastIndexAvailable());
    while (!simple_map.IsFastIndexAvailable())
       /*wait*/;
}

TEST(IndexedMap, LockFill)
{
    BkIdxMap<int,int> simple_map;
    simple_map.PerformLocked( [&simple_map](){
    for (int i = 0; i < 1000*1000; ++i)
        simple_map.Add(i, i+1);
    });
}

TEST(IndexedMap, Indexing)
{
    BkIdxMap<int,int> simple_map;
    for (int i = 0; i < 1000; ++i)
        simple_map.Add(i, i+1);
    EXPECT_FALSE(simple_map.IsFastIndexAvailable());
    EXPECT_EQ(simple_map.Find(-1), simple_map.End());
    EXPECT_EQ(simple_map.Find(1001), simple_map.End());
    for (int i = 0; i < 1000; ++i)
    {
        const auto found = simple_map.Find(i);
        ASSERT_EQ(found->first, i);
    }
    while (!simple_map.IsFastIndexAvailable())
       /*wait*/;
    for (int i = 0; i < 1000; ++i)
    {
        const auto found = simple_map.Find(i);
        ASSERT_EQ(found->first, i);
    }
    EXPECT_TRUE(simple_map.IsFastIndexAvailable());

    for (int i = 1000; i < 2000; ++i)
        simple_map.Add(i, i+1);
    EXPECT_EQ(simple_map.Find(-1), simple_map.End());
    EXPECT_EQ(simple_map.Find(2001), simple_map.End());
    for (int i = 0; i < 2000; ++i)
    {
        const auto found = simple_map.Find(i);
        ASSERT_EQ(found->first, i);
    }
}

//----------------- Timing Functions -------------------

static void FillLocked(benchmark::State &state)
{
    for (auto _ : state)
    {
        static BkIdxMap<int,int> map;
        map.PerformLocked( [&](){
            for (int i = 0; i < state.range(0); ++i)
                map.Add(i, i+1);
            });
    }
}

static void FillUnlocked(benchmark::State &state)
{
    for (auto _ : state)
    {
        static BkIdxMap<int,int> map;
        for (int i = 0; i < state.range(0); ++i)
            map.Add(i, i+1);
    }
}

static void IndexingTime(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.PauseTiming();
        static BkIdxMap<int,int> map;
        map.PerformLocked( [&](){
            for (int i = 0; i < state.range(0); ++i)
                map.Add(i, i+1);
            });
        state.ResumeTiming();
        while (!map.IsFastIndexAvailable())
            /*Wait*/;
    }
}

static void LookupNoIndex(benchmark::State &state)
{
    static BkIdxMap<int,int> map;
    if (map.Size()!=(size_t)state.range(0))
    {
        map.Clear();
        map.PerformLocked( [&](){
            for (int i = 0; i < state.range(0); ++i)
                map.Add(i, i+1);
            map.DisableIndexing();
            });
    }
    for (auto _ : state)
    {
        int i = rand()%state.range(0);
        benchmark::DoNotOptimize(map.Find(i));
    }
}


static void LookupIndexBuilt(benchmark::State &state)
{
    static BkIdxMap<int,int> map;
    if (map.Size()!=(size_t)state.range(0))
    {
        map.Clear();
        map.PerformLocked( [&](){
            for (int i = 0; i < state.range(0); ++i)
                map.Add(i, i+1);
            });
    }
    while (!map.IsFastIndexAvailable())
        ;
    for (auto _ : state)
    {
        int i = rand()%state.range(0);
        benchmark::DoNotOptimize(map.Find(i));
    }
}

static void LookupIndexMixed(benchmark::State &state)
{
    static BkIdxMap<int,int> map;
    if (map.Size()!=(size_t)state.range(0))
    {
        map.Clear();
        map.PerformLocked( [&](){
            for (int i = 0; i < state.range(0); ++i)
                map.Add(i, i+1);
            });
    }
    for (auto _ : state)
    {
        int i = rand()%state.range(0);
        benchmark::DoNotOptimize(map.Find(i));
    }
}

constexpr int MAX = 16*1000*1000;
constexpr int FILLMAX = 1*1000*1000;
BENCHMARK(FillLocked)->Range(FILLMAX, FILLMAX);
BENCHMARK(FillUnlocked)->Range(FILLMAX, FILLMAX);
BENCHMARK(IndexingTime)->Range(FILLMAX, FILLMAX);
BENCHMARK(LookupNoIndex)->Range(MAX, MAX);
BENCHMARK(LookupIndexBuilt)->Range(MAX, MAX);
BENCHMARK(LookupIndexMixed)->Range(MAX, MAX);
