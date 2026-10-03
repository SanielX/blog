---
title: "Testing performance of different struct layouts (W.I.P.)"
type: publications
date: 2026-10-01

affiliations:

---
{{< numbering h2=false h3=false >}}

Data-Oriented programming is big these days. It is basically a common knowledge at this point that DRAM is slow and that software should be designed to take advantage of CPU caches and prefetching. To make use of the latter - you need predictable memory access patterns. Which usually means iterating over arrays with a constant stride. Something you should also supposedly avoid is using "Fat Structs", i.e. structs that contain a lot of unrelated information in one place (such as game object with name, transform, flags and whatever else). However, fat structs are really simple to write and reason about. Most people will tell you that fat structs are perfectly fine to use, but I wanted to check for myself.

*TLDR: They are pretty bad! Even with linear access patterns.*
<br><br>
The reason for it is that, as it turns out, hardware prefetchers operate with "streams", and each of them is limited to a single page of memory (4 KiB usually). Iterating with big strides causes a program to cross page boundaries more often - leading to perfromance degradation. That said, more modern CPU designes do have an ability to prefetch across page boundaries, but I do not have such a CPU and expect most people not to have one either. Intel added this with Redwood Cove (2023).
<br><br>
With all that said I still wanted to get some concrete numbers for my machine to see if how much this stuff matters and at what scale. So that's what we're going to do. If you're interested in more in-depth info on this topic, check out the [references](#references) for this post.


## The Experiment {#the_experiment}

Let's say you're making a videogame. You have a list of entities considered "dirty", so you want to recompute transform matrix for each of them from respective position, rotation and scale. For the sake of simplicity, there will be no parents or children in the transform hierarchy. So what I do is as follows:
1. Prepare a list of dirty entity IDs (sequential or random list of all entities)
2. Compute transform matrix using 4 different layouts:
    * Fat Struct - Contains transform + output transform matrix + junk padding
    * Lean Struct - Contains transform + output transform matrix
    * Lean Struct (+Separate Output) - Entity contains only transform, output matrix is in a separate array
    * SOA - Each field is it's own array
3. Test it with 1'000, 10'000, 100'000, 400'000 and 1'000'000 entities
<br><br>

So here are the 4 struct layouts:
```c++
// 376 bytes per entity. Chosen arbitrarily
struct Entity_Fat 
{
  float4   rotation;
  float3   position;
  float3   scale;
  float4x4 transform;

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
float4x4                    *entities_lean_output;//<< note how we have separate output array
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
And here's is the loop we're going to test:
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
It is about the same for each struct layout so I will leave full version in the source code for brevity. `make_transform` is a normal function that actually computes a transform matrix. I thought it would be nice to have something at least a little resembling a real workload for the test.

## Results
I ran all these tests on AMD Ryzen 3700X. Each test ran 1000 iterations per method, but I also did run them multiple times and the numbers were about the same.
### Sequential Access - Median Time
<div id="chart_seq_median"></div>

So the results are about what I would expect. SOA is a little bit faster than any of the other ones, if I had to guess it could be because after loading 4 cache lines (index, position, rotation and scale) we can use them to compute ~16 matricies without going back into RAM. Multiple prefetch streams could also be helping. It's hard to tell for me.
It's also very apparent that just moving big matrix out to its own separate array already helped a ton, which is nice.

### Random Access - Median Time
<div id="chart_rand_median"></div>

This is not what I'd expected. But let me just point out the obvious first:
*Fat structs do not seem to matter when dealing with less than ~100'000 entities for both sequential and random access.* 
So yes, you can use them for your indie game just fine without hitting performance problems for a while.
<br><br>
Now with that said, what surprised me is SOA becoming about as slow as the Fat Struct when doing random access. I generally thought that SOA is supposed to just "be better" than everything else, but it very clearly isn't the case here. So what gives? <br>
Well, if we think about it, it actually makes sense. Remember how SOA needs to load 4 separate cache lines to compute the matrix? Now, we only use 1 value from each cache line and discarding the rest of it immediately. Compared to that, the Lean Struct approach only needs 1-2 cache lines to both compute and write the result.
<br><br>
If you run the benchmark with AMD uPerf we can find something else that I found interesting.

{{< figure src="../soa_testing/uprof_n400000_seq.jpg" class="expandable" alt="" caption="Sequential Access (400'000 entities)" >}}

{{< figure src="../soa_testing/uprof_n400000_rand.jpg" class="expandable" alt="" caption="Random Access (400'000 entities)" >}}

There is a sharp rise in L2 TLB cache misses. TLB is used to cache translation from virtual to physical addresses. When a miss occurs, the CPU has to invoke the Page Walker and search for the physical address by, well, walking the page table. Naturally, Fat Struct has a lot of misses because there are around 10 entities per page, so keeping anything in cache is very hard.
For SOA especially, we jump 5x as many pages per iteration, so naturally it produces more TLB misses.

## Conclusion {#conclusion}
Source code for the benchmark can be found [here](https://github.com/SanielX/blog/blob/master/content/publications/soa_testing/test.cpp).

The results of this tests were somewhat surprising to me and gave me a better view into cache performance considerations. Hopefully it was useful to you, the reader, as well.
Thanks for reading!

## References {#references}
1. ["What Every Programmer Should Know About Memory"](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) by Ulrich Drepper
2. [Battling the Prefetcher: Exploring Coffee Lake](https://abertschi.ch/blog/2022/prefetching/) by Bertschi Andrin
3. [Intel® 64 and IA-32 Architectures Optimization Reference Manual](https://www.intel.com/content/www/us/en/content-details/671488/intel-64-and-ia-32-architectures-optimization-reference-manual-volume-1.html)

<script>
var options = {
  series: [
    {
      name: 'Fat',
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
      name: 'SOA',
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
      name: 'Fat',
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
      name: 'SOA',
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