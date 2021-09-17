# background_index
A follow up to the sorted_vector timing example.
An example container that uses a background thread to create a fast-index for a large std::map
This should hopefully allow faster access if the index is up to date, but will largely not negatively impact the speed if the index is not available.

Timings from my (cheap) laptop
```
--------------------------------------------------------------------
Benchmark                          Time             CPU   Iterations
--------------------------------------------------------------------
FillLocked/1000000         156233145 ns    156234043 ns            4
FillUnlocked/1000000       207571674 ns    207569474 ns            3
IndexingTime/1000000        21686603 ns     21686357 ns           32
LookupIndexBuilt/16000000        540 ns          540 ns      1291817
LookupIndexMixed/16000000        558 ns          558 ns      1261220
LookupNoIndex/16000000          1491 ns         1491 ns       469293
```
So once the index is built the lookup is 3 times quicker (for a 16 million element structure). 
Not as good as the previous results would suggest, but having done this type of timing on many machines,
I was sceptical about how good they looked. In general, I would expect 2/7* speed-up.
Still not bad for a "free" technique (assuming you have memory and a hardware thread spare)
