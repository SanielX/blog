---
title: "Investigating struct layout performance"
type: publications
date: 2026-10-13
description: "Are fat structs fine?"

affiliations:

---
{{< numbering h2=false h3=false >}}

So I was trying to write a game from scratch. One of the first things I had to consider was whether I should bother doing an "Entity Component System" (ECS) or simply have a giant, "fat" struct with all the stuff packed into it. 
While a giant struct is easy to understand, the idea behind ECS is a little more sophisticated, but basically boils down to splitting a program's data into separate arrays to achieve better CPU cache utilization. 

If you're not familiar with the topic I recommend you watch *the talk* by Mike Acton on [Data Oriented Programming](https://www.youtube.com/watch?v=rX0ItVEVjHc). The post is written with an expectation that the reader already knows the basics of virtual memory and CPU caches.

The choice is essentially between an Array of Structs (AoS) and a Struct of Arrays (SoA). Supposedly, the latter is faster, and some will tell you to always prefer it over AoS. Online, I commonly see weird examples of SoA where vectors are stored like this:

```c++
struct Positions
{
  float *x;
  float *y;
  float *z;
}
```

It isn't particularly clear to me whether this is merely an example or serious advice. However, using SoA for maximum performance seems like the way to go. *Or is it?* Well, that's what I wanted to find out.

In this post, I want to take a simple task that you would realistically encounter when making a game, then create a bunch of different struct layouts and compare their performance, taking access patterns into account.

## The Setup {#the_test}

The task is as follows: you have a list of entities considered "dirty", perhaps given to you by the physics engine, you want to recompute the transform matrix for each entity using respective position, rotation and scale. For the sake of simplicity, there will be no parents or children in the transform hierarchy. Here's what I'm going to do:
1. Prepare a list of dirty entity IDs (sequential or random list of all entities).
2. Compute transform matrix using 4 different layouts:
    * Fat Struct - Contains transform, output transform matrix and junk padding.
    * Lean Struct - Contains transform and output transform matrix.
    * Lean Struct (+ Separate Output) - Entity contains only the transform data. The output matrix is stored in a separate array.
    * SoA - Each field is its own array.
3. Test it with 10'000, 100'000, 400'000 and 1'000'000 entities.


Here is the code for the 4 struct layouts:
```c++
// 376 bytes per entity. Chosen arbitrarily
struct Entity_Fat 
{
  float4   rotation;
  float3   position;
  float3   scale;
  float4x4 transform;
  // We simply pad the struct to pretend
  // that there is unrelated data in there
  char _junk_data_1[270];
};
Entity_Fat *entities_fat;
```

```c++
struct Entity_Lean
{
  float4   rotation;
  float3   position;
  float3   scale;
  float4x4 transform;
};
Entity_Lean *entities_lean;
```

```c++
struct Entity_Lean_Separate_Output
{
  float4 rotation;
  float3 position;
  float3 scale;
};
Entity_Lean_Separate_Output *entities_lean_input;
float4x4                    *entities_lean_output; // Note that we have a separate output array
```

```c++
struct Entity_SOA
{
  float4   *rotation;
  float3   *position;
  float3   *scale;
  float4x4 *transform;
};
Entity_SOA entities_soa;
```
And here is the loop we're going to test:
```c++
void test_fat()
{
  for(uint64_t i = 0; i < ENTITY_COUNT; i++)
  {
    int entity_index = entity_access_indices[i];

    Entity_Fat *entity = &entities_fat[entity_index];
    entity->transform = make_transform(entity->position, entity->rotation, entity->scale);
  }
}
```
The test function looks about the same for each struct layout so I will leave the full versions in the source code for brevity. `make_transform` is a procedure that computes a transform matrix. I thought it would be nice to have something at least somewhat resembling a real workload for the test.

I ran these tests on AMD Ryzen 3700X. Each test function is executed 1'000 times. I graphed only the median, although the program itself also computes min, max and the average values as well, in case you want to run it for yourself. The numbers seem pretty stable between program restarts as well, but they do depend on the mood of my computer on a particular day a little.

## Comparing fat structs
Before comparing different layouts I would like to point out something else I found interesting: the size of a struct greatly influences its performance characteristics, even for the linear access pattern.
### Linear Access - Median Time
<div id="chart_fat_struct_size_comparison"></div>

Wait but, I'm doing predictable memory access here, aren't I? I'm reading 1-2 cache lines of data and then store the result, how come the prefetcher doesn't help me here?

Well as it turns out, it is because hardware prefetchers operate with "streams". When at least 2 cache misses occur within a page, the CPU allocates a "prefetch stream" for *that particular page* and tries to predict which cache lines are gonna be read next. Note, that this works on a cache line granularity. As far as I can tell, the hardware does not care if your array accesses are done with stride of 1 or 2 bytes, it only sees that you have accessed two adjacent cache lines so it will try to load the next adjacent cache line. 

The important bit here is that each stream is limited to a *single page of memory* (4 KiB usually). Iterating with big strides causes a program to cross page boundaries more often - leading to performance degradation. Prefetching across page boundaries is also a thing, but it is pretty limited. From [this article](https://abertschi.ch/blog/2022/prefetching/#l2-streamer-prefetched-cache-lines-must-be-in-the-same-4k-page), it seems like CPU might preload first 2 cache lines of the next page. However, newer CPU designs such as Intel Redwood Cove have improved cross-page prefetchers, that "attempt to bring entire 4Kb page to the last level cache" (quote from Intel Optimization Reference Manual). Sadly, I do not have a CPU to test this with, but I wouldn't count on this behavior when targeting the general audience.

Lastly I want to point out, that the 2048 byte struct is significantly slower than the other 2 versions even when doing only 10'000 operations, It's hard to see, but the difference is 0.2 ms vs 0.06 ms

## Comparing data layouts
### Linear Access - Median Time
<div id="chart_seq_median"></div>

This is exactly what I expected to see. SoA is a little bit faster than any of the other ones. My understanding is that, even though we load 4 separate cache lines (index, position, rotation and scale), they can be used to compute ~16 matrices without going back into RAM, compared to "Lean Struct" where 40 bytes of a single cache line are used to compute just one transform matrix. I speculate that multiple prefetch streams could also be helping SoA.

### Random Access - Median Time
<div id="chart_rand_median"></div>

Interesting. SoA is becoming about as slow as the Fat Struct when doing random access. I generally thought that SoA is supposed to just "be better" than everything else, but it very clearly isn't the case here. So what gives? 

Well, if we think about it, remember how SoA needs to load 4 separate cache lines to compute the transform matrix? Now, we only use 1 value from each cache line and discarding the rest of it immediately. Compared to that, the Lean Struct layout only needs to read ~2 cache lines to both compute and write the result.

Digging a little deeper, I've decided to try using AMD uPerf (VTune would be the equivalent for Intel) and found something else.

{{< figure src="../soa_testing/uprof_n400000_seq.jpg" class="expandable" alt="" caption="Sequential Access (400'000 entities)" >}}

{{< figure src="../soa_testing/uprof_n400000_rand.jpg" class="expandable" alt="" caption="Random Access (400'000 entities)" >}}

There is a sharp rise in L2 TLB misses! The Translation Lookaside Buffer (TLB) is used to cache mappings from virtual to physical addresses. When a miss occurs, the CPU has to invoke the Page Walker and search for the physical address by, well, walking the page table. Naturally, the Fat Struct layout experiences a lot of misses because there are only around 10 entries per page, making it difficult for the CPU to keep data in cache.
With SoA specifically, we jump 5 times as many pages per iteration, compared to the Lean Struct version, so it naturally produces more TLB misses.

## Conclusion {#conclusion}
It seems pretty apparent to me that fat structs are indeed fine until they get too big or until you have too many things to process at once. Although, "a lot" here stands for "around 100'000". All and all, using fat structs as a starting point is pretty reasonable.
If you have to do a lot of random accesses then it might be a good idea to make your data as small as possible and avoid constantly jumping between pages.<br>
Nowhere in this blog have I discussed what happens if you run this code using multiple threads, where you have to start caring about false sharing, among other issues. Perhaps I will explore that next time.

The source code for the benchmark can be found [here](https://github.com/SanielX/blog/blob/master/content/publications/soa_testing/test.cpp).

I learned something new doing this, hopefully it was useful to you, the reader, as well.
Thanks for following along, see you in the next one!

## References {#references}
1. ["What Every Programmer Should Know About Memory"](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) by Ulrich Drepper
2. [Battling the Prefetcher: Exploring Coffee Lake](https://abertschi.ch/blog/2022/prefetching/) by Bertschi Andrin
3. [Intel® 64 and IA-32 Architectures Optimization Reference Manual](https://www.intel.com/content/www/us/en/content-details/671488/intel-64-and-ia-32-architectures-optimization-reference-manual-volume-1.html)


<script>
var options = {
  series: [
    {
      name: 'Fat (376 bytes)',
      data: [0.075000, 2.295000, 9.666000, 25.834000],
    },
    {
      name: 'Lean',
      data: [0.071000, 0.715000, 3.556000, 9.869000],
    },
    {
      name: 'Lean (Separate Output)',
      data: [0.073000, 0.720000, 3.241000, 8.378000],
    },
    {
      name: 'SoA',
      data: [0.067000, 0.681000, 3.073000, 8.215000],
    },
  ],
  chart: {
    type: 'bar',
    height: 350,
  },
  plotOptions: {
    bar: {
      horizontal: false,
      columnWidth: '55%',
      borderRadius: 5,
      borderRadiusApplication: 'end',
    },
  },
  dataLabels: {
    enabled: false,
  },
  stroke: {
    show: true,
    width: 2,
    colors: ['transparent'],
  },
  xaxis: {
    categories: ['10\'000', '100\'000', '400\'000', '1\'000\'000'],
  },
  yaxis: {
    logarithmic: false,
    title: {
      text: 'milliseconds',
    },
  },
  fill: {
    opacity: 1,
  },
  tooltip: {
    y: {
      formatter: function (val) {
        return val + " ms";
      },
    },
  },
}
var chart = new ApexCharts(document.querySelector('#chart_seq_median'), options)
chart.render()
</script>

<script>
var options = {
  series: [
    {
      name: 'Fat (376 bytes)',
      data: [0.089000, 4.004000, 17.447000, 45.502000],
    },
    {
      name: 'Lean',
      data: [0.085000, 0.907000, 11.146000, 29.932000],
    },
    {
      name: 'Lean (Separate Output)',
      data: [0.091000, 0.987000, 16.182000, 42.904000],
    },
    {
      name: 'SoA',
      data: [0.089000, 1.020000, 16.006000, 42.206000],
    },
  ],
  chart: {
    type: 'bar',
    height: 350,
  },
  plotOptions: {
    bar: {
      horizontal: false,
      columnWidth: '55%',
      borderRadius: 5,
      borderRadiusApplication: 'end',
    },
  },
  dataLabels: {
    enabled: false,
  },
  stroke: {
    show: true,
    width: 2,
    colors: ['transparent'],
  },
  xaxis: {
    categories: ['10\'000', '100\'000', '400\'000', '1\'000\'000'],
  },
  yaxis: {
    logarithmic: false,
    title: {
      text: 'milliseconds',
    },
  },
  fill: {
    opacity: 1,
  },
  tooltip: {
    y: {
      formatter: function (val) {
        return val + " ms";
      },
    },
  },
}
var chart = new ApexCharts(document.querySelector('#chart_rand_median'), options)
chart.render()
</script>


<script>
var options = {
  series: [
    { name: '128 bytes',  data: [0.053000, 0.555000, 4.129000, 10.306000], }, 
    { name: '512 bytes',  data: [0.060000, 1.760000, 7.271000, 18.284000], }, 
    { name: '2048 bytes', data: [0.213000, 2.350000, 9.671000, 25.359000], }, 
  ],
  chart: {
    type: 'bar',
    height: 350,
  },
  plotOptions: {
    bar: {
      horizontal: false,
      columnWidth: '55%',
      borderRadius: 5,
      borderRadiusApplication: 'end',
    },
  },
  dataLabels: {
    enabled: false,
  },
  stroke: {
    show: true,
    width: 2,
    colors: ['transparent'],
  },
  xaxis: {
    categories: ["10'000", "100'000", "400'000", "1'000'000"],
  },
  yaxis: {
    logarithmic: false,
    title: {
      text: 'milliseconds',
    },
  },
  fill: {
    opacity: 1,
  },
  tooltip: {
    y: {
      formatter: function (val) {
        return val + " ms";
      },
    },
  },
}
var chart = new ApexCharts(document.querySelector('#chart_fat_struct_size_comparison'), options)
chart.render()
</script>