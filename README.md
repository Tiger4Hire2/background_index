# background_index
A follow up to the sorted_vector timing example.
An example container that uses a background thread to create a fast-index for a large std::map
This should hopefully allow faster access if the index is up to date, but will largely not negatively impact the speed if the index is not available.

Timings from my (cheap) laptop : Update, this is now using a two layer index.
```
--------------------------------------------------------------------
Benchmark                          Time             CPU   Iterations
--------------------------------------------------------------------
FillLocked/1000000         138139344 ns    138135719 ns            5
FillUnlocked/1000000       144124612 ns    144126472 ns            5
IndexingTime/1000000        21456230 ns     21455934 ns           32
LookupIndexBuilt/16000000        345 ns          345 ns      2032166
LookupIndexMixed/16000000        380 ns          380 ns      1855948
LookupNoIndex/16000000          1500 ns         1500 ns       470086
```
So once the index is built the lookup is 3 times quicker (for a 16 million element structure). 
Not as good as the previous results would suggest, but having done this type of timing on many machines,
I was sceptical about how good they looked. In general, I would expect 2/7* speed-up.
Still not bad for a "free" technique (assuming you have memory and a hardware thread spare)
