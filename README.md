# background_index
A follow up to the sorted_vector timing example.
An example container that uses a background thread to create a fast-index for a large std::map
This should hopefully allow faster access if the index is up to date, but will largely not negatively impact the speed if the index is not available.

Timings from my (cheap) laptop : 
Update: this is now using a two layer index.
Update: this is now has performance counter support 
```
------------------------------------------------------------------------------------
Benchmark                          Time             CPU   Iterations UserCounters...
------------------------------------------------------------------------------------
FillLocked/1000000         147184979 ns    147178306 ns            5 L1-dcache-load-misses=20.5997M branch-misses=1.75415M cycles=1.1371G
FillUnlocked/1000000       144459606 ns    144452759 ns            5 L1-dcache-load-misses=-2.52191M branch-misses=-431.245M cycles=431.091M
IndexingTime/1000000        19212882 ns     19212197 ns           36 L1-dcache-load-misses=-308.615k branch-misses=-37.373M cycles=36.2313k
LookupNoIndex/16000000          1306 ns         1305 ns       535761 L1-dcache-load-misses=4.00897k branch-misses=4.0035k cycles=4.0035k
LookupIndexBuilt/16000000        387 ns          387 ns      1829548 L1-dcache-load-misses=7.29095 branch-misses=-1.16597k cycles=-1.17514k
LookupIndexMixed/16000000        390 ns          390 ns      1808678 L1-dcache-load-misses=-1.90074 branch-misses=1.1859k cycles=1.18594k
```
So once the index is built the lookup is 3 times quicker (for a 16 million element structure). 
Not as good as the previous results would suggest, but having done this type of timing on many machines,
I was sceptical about how good they looked. In general, I would expect 2/7* speed-up.
Still not bad for a "free" technique (assuming you have memory and a hardware thread spare)
