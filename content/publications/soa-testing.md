---
title: "Testing performance of different struct layouts (W.I.P.)"
type: publications
date: 2026-10-01

affiliations:

---
{{< numbering h2=false h3=false >}}

Data-Oriented programming is big these days. It is baiscally common knowledge at this point that RAM is slow and that your software should be designed to take advantage of CPU caches and prefetching. To take advantage of the latter, you need predictable memory access patterns. By predictable people usually mean iterating over arrays with constant stride. 
<br><br>
This relates somewhat to the idea of using "Struct of Arrays" intead of "Array of Structs" to improve cache locality of your data and prefetching. The questions I always got is: 
- How come CPU doesn't get confused when I'm jumping from one array to the next? 
- What kinds of memory access patterns are considered predictable?
- Are fat structs really that bad for performance?
<br><br>
In this blog post I want to setup a simple experiment to test different data layouts and see some numbers. I think nothing I present here is big news, you can find more in-depth info on this topic in [References](#references)


## The Point {#the_point}
Hardware prefetcher operates on cache lines, not simply memory addresses. It also has multiple "streams" that operate independently within an OS page (usually 4 KiB in size). Prefetching across page boundaries exists only on newer CPU architectures such as Intel's Redwood Cove.

## The Experiment {#the_experiment}

Let's say you're making a videogame and your physics engine has given you a list of entity IDs that have moved this frame. You want to take this list and compute a transform matrix for each entity from their position, rotation and scale. For the sake of simplicity, let's say that the transform hierarchy is completely flat, no parents or children.
<br><br>

```c++
struct Entity_Fat // 376 bytes per entity
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


## Results
I ran all these tests on AMD Ryzen 3700X. Each test ran 1000 iterations per method, but I also did run them multiple times and the numbers were about the same.
### Sequential Access - Median Time
<div id="chart_seq_median"></div>

### Random Access - Median Time
<div id="chart_rand_median"></div>

Preliminary results seems to indicate the following:
- Fat structs literally do not matter when dealing with less than ~100'000 entities
- SOA predictably wins with sequential access, although not by as much as I would expect compared to the "Separate Output" version

Now there is an elephant in the room worth addressing, which is that both SOA and Separate Output become about as bad as the Fat Struct when doing tons of random accesses.
Initially I was very confused by this, but after spinning up AMD uPerf and looking at some numbers it began to make sense to me.
<br><br>
What the profiler shows, among other things, is that we have an astronomical amount of L2 TLB cache misses, which are the slow kind. From Intel Performance manual: "A miss in the shared TLB results in the Page Walker being invoked and this penalty can be noticeable in the execution."

{{< figure src="../soa_testing/uprof_n400000_seq.jpg" class="expandable" alt="" caption="Sequential Access (400'000 entities)" >}}

{{< figure src="../soa_testing/uprof_n400000_rand.jpg" class="expandable" alt="" caption="Random Access (400'000 entities)" >}}

Naturally, Fat version sucks because there are around 10 entities per page so keeping anything in cache is very hard.
However, both SOA and Separate Output are problematic because **the CPU has to access multiple separate pages before it can do any work**. Compared to that, the lean version reads data from a cache line, computes the matrix and writes data back to the same/adjacent cache line (and adjacent cache line is usually in L2 already).

## Conclusion {#conclusion}
I think the biggest lesson here is just another proof that performance of anything will heavily depend on the workload and the algorithm. 
It was a surprise for me to see SOA be so slow with random access. It is pretty apparent that keeping your struct size low is benefitial even if you access your data randomly.

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
      text: 'milliseconds (logarithmic)',
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
      text: 'milliseconds (logarithmic)',
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