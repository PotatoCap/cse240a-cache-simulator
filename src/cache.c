//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <stdio.h>

//
// TODO:Student Information
//
const char *studentName = "Valerie Liu";
const char *studentID   = "A14895768";
const char *email       = "rul021@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
uint32_t offset_bits;
uint32_t i_index_bits;
uint32_t d_index_bits;
uint32_t l2_index_bits;

struct block {
  uint32_t tag;
  int has_data;
  int lru;
};

struct block** icache;
struct block** dcache;
struct block** l2cache;

enum cacheType {ic, dc, l2};
//

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

uint32_t intlog2 (uint32_t n) {
  if (n == 0) return 0;
  uint32_t result = 0;
  while (n > 1) {
    n >>= 1;
    result++;
  }
  return result;
}

uint32_t getIndex(uint32_t addr, uint32_t index_bits){
  uint32_t higher_bits = addr >> offset_bits;
  uint32_t index_mask = (1 << index_bits) - 1;
  return higher_bits & index_mask;
}

uint32_t getTag(uint32_t addr, uint32_t index_bits){
  return addr >> (offset_bits + index_bits);
}

int readData(struct block* line, uint32_t assoc, uint32_t tag){
  for (int i = 0; i < assoc; i++)
  {
    struct block data = line[i];
    if (data.tag == tag)
    {
      return data.has_data;
    }
  }
  return FALSE;
}

void updateLRU(struct block* line, uint32_t assoc, uint32_t tag){
  for (int i = 0; i < assoc; i++)
  {
    struct block data = line[i];
    if (data.tag == tag) {
      data.lru = 0;
    }else{
      data.lru++;
    }
  }
}

void insertData(struct block* line, uint32_t assoc, uint32_t tag){
  int maxlru = 0;
  int maxarg = 0;
  for (int i = 0; i < assoc; i++)
  {
    struct block data = line[i];
    if (!data.has_data) {
      maxarg = i;
      break;
    }else if(data.lru > maxlru) {
      maxlru = data.lru;
      maxarg = i;
    }
  }
  line[maxarg].has_data = TRUE;
  line[maxarg].tag = tag;
  updateLRU(line, assoc, tag);
}

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  offset_bits = intlog2(blocksize);
  i_index_bits = intlog2(icacheSets);
  d_index_bits = intlog2(dcacheSets);
  l2_index_bits = intlog2(l2cacheSets);

  icache = malloc(icacheSets * sizeof(struct block*));
  for (int i = 0; i < icacheSets; i++)
  {
    icache[i] = malloc(icacheAssoc * sizeof(struct block));
    for (int j = 0; j < icacheAssoc; j++)
    {
      icache[i][j] = (struct block){0, 0, 0};
    }
  }

  dcache = malloc(dcacheSets * sizeof(struct block*));
  for (int i = 0; i < dcacheSets; i++)
  {
    dcache[i] = malloc(dcacheAssoc * sizeof(struct block));
    for (int j = 0; j < dcacheAssoc; j++)
    {
      dcache[i][j] = (struct block){0, 0, 0};
    }
  }

  l2cache = malloc(l2cacheSets * sizeof(struct block*));
  for (int i = 0; i < icacheSets; i++)
  {
    l2cache[i] = malloc(l2cacheAssoc * sizeof(struct block));
    for (int j = 0; j < l2cacheAssoc; j++)
    {
      l2cache[i][j] = (struct block){0, 0, 0};
    }
  }
}

int
cache_access(enum cacheType type, uint32_t addr){
  struct block** cache;
  uint32_t assoc;
  uint32_t index_bits;

  switch (type) {
    case ic:
      cache = icache;
      assoc = icacheAssoc;
      index_bits = i_index_bits;
      break;
    case dc:
      cache = dcache;
      assoc = dcacheAssoc;
      index_bits = d_index_bits;
      break;
    case l2:
      cache = l2cache;
      assoc = l2cacheAssoc;
      index_bits = l2_index_bits;
      break;
    default:
      printf("Error: unknown cache type %d\n", type);
      exit(1);
  }

  uint32_t index = getIndex(addr, index_bits);
  uint32_t tag = getTag(addr, index_bits);
  struct block* line = cache[index];
  int hit = readData(line, assoc, tag);
  if (hit) {
    updateLRU(line, assoc, tag);
  } else {
    insertData(line, assoc, tag);
  }
  return hit;
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  icacheRefs++;
  int hit = cache_access(ic, addr);
  if (hit) {
    return icacheHitTime;
  } else {
    icacheMisses++;
    uint32_t penalty = l2cache_access(addr);
    icachePenalties += penalty;
    return icacheHitTime+penalty;
  }
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  dcacheRefs++;
  int hit = cache_access(dc, addr);
  if (hit) {
    return dcacheHitTime;
  } else {
    dcacheMisses++;
    uint32_t penalty = l2cache_access(addr);
    dcachePenalties += penalty;
    return dcacheHitTime + penalty;
  }
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  l2cacheRefs++;
  int hit = cache_access(l2, addr);
  if (hit) {
    return l2cacheHitTime;
  } else {
    l2cacheMisses++;
    l2cachePenalties += memspeed;
    return l2cacheHitTime + memspeed;
  }
}
