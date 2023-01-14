////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_cache.c
//  Description    : This is the cache implementation for the LionCloud
//                   assignment for CMPSC311.
//
//   Author        : Tzu Chieh Huang
//   Last Modified : 18th Apr 2020
//

// Includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmpsc311_log.h>
#include <lcloud_cache.h>

//create the storage
typedef struct {
    LcDeviceId did;
    uint16_t sec;
    uint16_t blk;
    uint16_t lru;
    char data[LC_DEVICE_BLOCK_SIZE];
} storage;

// needed cache storage
storage* cache = NULL;
int max_blocks;
uint16_t lru = 0;
int hit_count;
int miss_count;

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_getcache
// Description  : Search the cache for a block
//
// Inputs       : did - device number of block to find
//                sec - sector number of block to find
//                blk - block number of block to find
// Outputs      : cache block if found (pointer), NULL if not or failure

char* lcloud_getcache(LcDeviceId did, uint16_t sec, uint16_t blk)
{
    int i;

    if (cache == NULL) {
        return NULL;
    }

    for (i = 0; i < max_blocks; i++) {
	// check the right device, block and sector
        if (cache[i].did == did && cache[i].sec == sec && cache[i].blk == blk) {
		//get into LRU, and return the data 
            cache[i].lru = lru++;
            hit_count++;
            return cache[i].data;
        }
    }
	// if no correct data, get miss count 
    miss_count++;

    /* Return not found */
    return (NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_putcache
// Description  : Put a value in the cache
//
// Inputs       : did - device number of block to insert
//                sec - sector number of block to insert
//                blk - block number of block to insert
// Outputs      : 0 if succesfully inserted, -1 if failure

int lcloud_putcache(LcDeviceId did, uint16_t sec, uint16_t blk, char* block)
{
    int i, v;

    if (cache == NULL) {
        return -1;
    }

	// select the empty cache (-1 for false, no device means no data in the cache)
    v = -1;
    for (i = 0; i < max_blocks; i++) {
        if (cache[i].did == -1) {
            v = i;
            break;
        }
        if (v == -1) {
            v = i;
	//if there is no empty cache, find the smallest cacheline to change it
        	} else if (lru - cache[i].lru < lru - cache[v].lru) { 
            v = i;
        }
    }
    // copy the data to the cache
    cache[v].did = did;
    cache[v].sec = sec;
    cache[v].blk = blk;
    cache[v].lru = lru++;
    memcpy(cache[v].data, block, LC_DEVICE_BLOCK_SIZE);

    /* Return successfully */
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_initcache
// Description  : Initialze the cache by setting up metadata a cache elements.
//
// Inputs       : maxblocks - the max number number of blocks
// Outputs      : 0 if successful, -1 if failure

int lcloud_initcache(int maxblocks)
{
    // malloc the catch, and reset the storage
    cache = (storage*)malloc(sizeof(storage) * maxblocks);
    max_blocks = maxblocks;
    lru = 0;
    hit_count = 0;
    miss_count = 0;
    memset(cache, -1, sizeof(storage) * maxblocks);

    /* Return successfully */
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcloud_closecache
// Description  : Clean up the cache when program is closing
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int lcloud_closecache(void)
{
    int total;
    double ratio;

    if (cache == NULL) {
        return -1;
    }

    total = hit_count + miss_count;
    ratio = (double)hit_count / total;
    logMessage(LOG_OUTPUT_LEVEL, "Hits/Misses/Total: %d/%d/%d\n", hit_count, miss_count, total);
    logMessage(LOG_OUTPUT_LEVEL, "Hit Ratio: %lf\n", ratio);
    // free the cache storage
    free(cache);

    /* Return successfully */
    return (0);
}
