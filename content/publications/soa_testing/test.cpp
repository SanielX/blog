// Build with 'cl test.cpp -O2'
// Fat struct test commands:
/*
-E[NUMBER] - sets size of a fat struct
-N[NUMBER] - sets entity count
-rand      - switches to random access patterns
-sort      - attempts to do std::sort when doing random access (just makes everything slower tbh)
*/

#include <corecrt_malloc.h>
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include <intrin.h>
#include <time.h>
#include <algorithm>

union float3
{
  struct { float x, y, z; };
  float v[3];
};
union float4
{
  struct { float x, y, z, w; };
  float v[4];

  float& operator [](int index) { return v[index]; }
};
union float4x4
{
  struct { float4 r0, r1, r2, r3; };
  float4 v[4];

  float4& operator [](int index) { return v[index]; }
};

inline float4x4 make_transform(float3 t, float4 r, float3 s)
{
  float4x4 o;
  o[0][0] = (1.0f - 2.0f * (r.y * r.y + r.z * r.z)) * s.x;
  o[0][1] = (r.x * r.y - r.z * r.w) * s.y * 2.0f;
  o[0][2] = (r.x * r.z + r.y * r.w) * s.z * 2.0f;
  o[0][3] = t.x;

  o[1][0] = (r.x * r.y + r.z * r.w) * s.x * 2.0f;
  o[1][1] = (1.0f - 2.0f * (r.x * r.x + r.z * r.z)) * s.y;
  o[1][2] = (r.y * r.z - r.x * r.w) * s.z * 2.0f;
  o[1][3] = t.y;

  o[2][0] = (r.x * r.z - r.y * r.w) * s.x * 2.0f;
  o[2][1] = (r.y * r.z + r.x * r.w) * s.y * 2.0f;
  o[2][2] = (1.0f - 2.0f * (r.x * r.x + r.y * r.y)) * s.z;
  o[2][3] = t.z;
  
  o[3][0] = 0;
  o[3][1] = 0;
  o[3][2] = 0;
  o[3][3] = 1;

  return o;
}

size_t ENTITY_FAT_SIZE = 376; // to make testing easier this is also configurable. 
struct Entity_Fat // 376 bytes per entity
{
  float4   rotation;
  float3   position;
  float3   scale;
  float4x4 transform;
};
Entity_Fat *entities_fat;

__declspec(noinline) void init_entity_fat(uint64_t entity_count)
{
  size_t fat_entity_size = (sizeof(Entity_Fat) + (ENTITY_FAT_SIZE - sizeof(Entity_Fat)));
  printf("Fat Entity size %llu\n", fat_entity_size);
  entities_fat = (Entity_Fat*)_aligned_malloc(fat_entity_size * entity_count, 4096);
  for(uint64_t i = 0; i < entity_count; i++)
  {
    entities_fat[i] = 
    {
      .rotation = { 0, 0, 0, 1},
      .position = { .x = float(i), .y = float(-i), .z = 0 },
      .scale    = { 1, 1, 1 },
    };
  }
}

struct Entity_Lean
{
  float4   rotation;
  float3   position;
  float3   scale;
  float4x4 transform;
};
Entity_Lean *entities_lean;

__declspec(noinline) void init_entity_lean(uint64_t entity_count)
{
  entities_lean = (Entity_Lean*)_aligned_malloc(sizeof(Entity_Lean) * entity_count, 4096);
  for(uint64_t i = 0; i < entity_count; i++)
  {
    entities_lean[i] = 
    {
      .rotation = { 0, 0, 0, 1},
      .position = { .x = float(i), .y = float(-i), .z = 0 },
      .scale    = { 1, 1, 1 },
    };
  }
}

struct Entity_Lean_Separate_Output
{
  float4 rotation;
  float3 position;
  float3 scale;
};
Entity_Lean_Separate_Output *entities_lean_input;
float4x4                    *entities_lean_output;

__declspec(noinline) void init_entity_lean_separate_output(uint64_t entity_count)
{
  entities_lean_input  = (Entity_Lean_Separate_Output*)_aligned_malloc(sizeof(Entity_Lean_Separate_Output) * entity_count, 4096);
  entities_lean_output = (float4x4*)                   _aligned_malloc(sizeof(float4x4) * entity_count, 4096);
  for(uint64_t i = 0; i < entity_count; i++)
  {
    entities_lean_input[i] = 
    {
      .rotation = { 0, 0, 0, 1},
      .position = { .x = float(i), .y = float(-i), .z = 0 },
      .scale    = { 1, 1, 1 },
    };
    entities_lean_output[i] = {};
  }
}

struct Entity_SOA
{
  float4   *rotation;
  float3   *position;
  float3   *scale;
  float4x4 *transform;
};
Entity_SOA entities_soa;

__declspec(noinline) void init_entity_soa(uint64_t entity_count)
{
  entities_soa.position  = (float3*)  _aligned_malloc(sizeof(float3)   * entity_count, 4096);
  entities_soa.scale     = (float3*)  _aligned_malloc(sizeof(float3)   * entity_count, 4096);
  entities_soa.rotation  = (float4*)  _aligned_malloc(sizeof(float4)   * entity_count, 4096);
  entities_soa.transform = (float4x4*)_aligned_malloc(sizeof(float4x4) * entity_count, 4096);
  for(uint64_t i = 0; i < entity_count; i++)
  {
    entities_soa.position [i] = { .x = float(i), .y = float(-i), .z = 0 };
    entities_soa.rotation [i] = { 0, 0, 0, 1};
    entities_soa.scale    [i] = { 1, 1, 1};
    entities_soa.transform[i] = {0}; // touch this memory to avoid page faults
  }
}

size_t ENTITY_COUNT = 2'500'000;

void test_fat();
void test_lean();
void test_lean_separate_output();
void test_soa();

LARGE_INTEGER clock_frequency;

uint64_t sample_time() 
{
  LARGE_INTEGER clock_value;
  QueryPerformanceCounter(&clock_value);

  return clock_value.QuadPart;
}

double elapsed_miliseconds(uint64_t start, uint64_t end)
{
  // assert(start <= end);
	uint64_t elapsed = end - start;
  uint64_t elapsedMicroseconds = (elapsed * 1'000'000) / clock_frequency.QuadPart;
  return   elapsedMicroseconds * 1e-3;
}

int *entity_access_indices;
int *entity_access_indices_original;

__declspec(noinline) void init_access_indices(uint64_t entity_count, bool random_pattern)
{
  entity_access_indices          = (int*)_aligned_malloc(sizeof(int)*entity_count, 4096);
  entity_access_indices_original = (int*)_aligned_malloc(sizeof(int)*entity_count, 4096);
  
  for(uint64_t i = 0; i < entity_count; i++) // linear access
  {
    entity_access_indices[i] = i; 
  }
  
  
  if(random_pattern)
  {
    int N = entity_count;
    for(int i = N-1; i >= 1; i--)
    {
      int j = rand();
      while(j > i) j = rand();

      std::swap(entity_access_indices[i], entity_access_indices[j]);
    }
    for(int i = N-1; i >= 1; i--)
    {
      int j = rand();
      while(j > i) j = rand();
      
      std::swap(entity_access_indices[i], entity_access_indices[j]);
    }
  }

  memcpy(entity_access_indices_original, entity_access_indices, sizeof(int)*entity_count);
}

using bench_func = void();
void bench(const char *what, uint64_t iterations, bench_func *func);

bool RANDOM_INDICES = false;
bool DO_SORT        = false;

int main(int argc, char **argv)
{
  srand(time(NULL));
  QueryPerformanceFrequency(&clock_frequency);
  
  for(int i = 0; i < argc; i++)
  {
    if(strcmp(argv[i], "-rand") == 0)
    {
      RANDOM_INDICES = true;
    }
    else if(strcmp(argv[i], "-sort") == 0)
    {
      DO_SORT = true;
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'N')
    {
      ENTITY_COUNT = atoi(argv[i]+2);
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'E')
    {
      ENTITY_FAT_SIZE = atoi(argv[i]+2);
    }
  }

  init_access_indices             (ENTITY_COUNT, RANDOM_INDICES);
  printf("Testing with %llu entities...\n", ENTITY_COUNT);

  init_entity_fat                 (ENTITY_COUNT);
  init_entity_lean                (ENTITY_COUNT);
  init_entity_lean_separate_output(ENTITY_COUNT);
  init_entity_soa                 (ENTITY_COUNT);

  uint64_t iterations = 1000;
  printf("|Struct Layout                | Avg (ms) | Median (ms) | Min (ms) | Max (ms) | Sort Avg (ms) |\n");
  bench ("Fat                          ", iterations, test_fat); 
  bench ("Lean                         ", iterations, test_lean);
  bench ("Lean (Separate Output)       ", iterations, test_lean_separate_output);
  bench ("SOA                          ", iterations, test_soa);
}

__declspec(noinline) void cache_evict_indices()
{
  if(DO_SORT)
  {
    memcpy(entity_access_indices, entity_access_indices_original, sizeof(int)*ENTITY_COUNT);
  }

  constexpr size_t CACHE_LINE = 64;
  uint8_t *addr = (uint8_t*)entity_access_indices;
  for(uint64_t i = 0; i < ENTITY_COUNT; i += CACHE_LINE)
  {
    _mm_clflush(addr);
    addr += CACHE_LINE;
  }
}

volatile uint64_t time_stub;

__declspec(noinline) void bench(const char *what, uint64_t iterations, bench_func *func)
{
  double *results      = (double*)malloc(sizeof(double) * iterations);

  double t_sort_avg = 0.0;
  double t_avg = 0.0, t_min = 999999999, t_max = -99999999;
  for(uint64_t i = 0; i < iterations; i++)
  {
    cache_evict_indices();

    uint64_t t_start = sample_time();
    
    if(DO_SORT)
    {
      std::sort(entity_access_indices, entity_access_indices+ENTITY_COUNT);
      uint64_t t_sort_end = sample_time();
      t_sort_avg += elapsed_miliseconds(t_start, t_sort_end);
    }

    func();
    
    uint64_t t_end = sample_time();

    double   elapsed = elapsed_miliseconds(t_start, t_end);
    results[i] = elapsed;
    t_avg     += elapsed;
  }
  t_avg    /= (double)iterations;
  t_sort_avg /= (double)iterations;

  std::sort(results, results + iterations);
  double t_median = results[iterations / 2];

  int offset = (int)((iterations * 0.01) / 2); // Do 99%
  for(uint64_t i = offset; i < iterations-offset; i++)
  {
    double elapsed = results[i];
    t_min = elapsed < t_min? elapsed : t_min;
    t_max = elapsed > t_max? elapsed : t_max;
  }
  
  printf("|%s| %f | %f    | %f | %f | %f |\n", what, t_avg, t_median, t_min, t_max, t_sort_avg);

  free(results);
}

void test_fat()
{
  for(uint64_t i = 0; i < ENTITY_COUNT; i++)
  {
    int entity_index = entity_access_indices[i];

    Entity_Fat *entity = (Entity_Fat*)((char*)entities_fat + ENTITY_FAT_SIZE * i);
    entity->transform = make_transform(entity->position, entity->rotation, entity->scale);
  }
}

void test_lean()
{
  for(uint64_t i = 0; i < ENTITY_COUNT; i++)
  {
    int entity_index = entity_access_indices[i];

    Entity_Lean *entity = &entities_lean[entity_index];
    entity->transform = make_transform(entity->position, entity->rotation, entity->scale);
  }
}

void test_lean_separate_output()
{
  for(uint64_t i = 0; i < ENTITY_COUNT; i++)
  {
    int entity_index = entity_access_indices[i];

    Entity_Lean_Separate_Output *entity = &entities_lean_input[entity_index];
    entities_lean_output[entity_index] = make_transform(entity->position, entity->rotation, entity->scale);
  }
}

void test_soa()
{
  for(uint64_t i = 0; i < ENTITY_COUNT; i++)
  {
    int entity_index = entity_access_indices[i];

    entities_soa.transform[entity_index] = make_transform(entities_soa.position[entity_index], 
                                                          entities_soa.rotation[entity_index], 
                                                          entities_soa.scale   [entity_index]);
  }
}
