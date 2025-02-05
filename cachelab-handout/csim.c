#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

typedef unsigned long int uint64_t;

typedef struct 
{
 int valid;
 uint64_t tag;
 int lru;  
}cacheLine;

typedef cacheLine* cacheSet;
typedef cacheSet* Cache;

const char* usage = "Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n";

int verbose = 0; //verbose flag 
int s;  //number of set index bits 
int E;  //number of lines per set
int b;  //number of block bits

FILE* fp = NULL;
Cache cache;

int misses = 0;
int hits = 0;
int evictions = 0;

void parseArgument(int argc, char* argv[]);
int visitCache(uint64_t address);
int simulate();


int main(int argc, char* argv[])
{
    parseArgument(argc, argv);
    simulate();
    printSummary(hits, misses, evictions);
    return 0;
}

void parseArgument(int argc, char* argv[])
{
    int opt;
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {        
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                fp = fopen(optarg, "r");
                break;
            case 'h':
            default:
                fprintf(stdout, usage, argv[0]);
                exit(1);
        }
    }
}

int simulate()
{
    int S = pow(2, s);
    cache = (Cache)malloc(sizeof(cacheSet) * S);
    if(cache == NULL) return -1;
    for(int i=0;i < S; i++)
    {
        cache[i] = (cacheSet)calloc(E, sizeof(cacheLine));
        if(cache[i]==NULL) return -1;
    }

    char buf[20];
    char opreation;
    uint64_t address;
    int size;

    while(fgets(buf, sizeof(buf), fp) != NULL)
    { 
        int ret;

        if(buf[0] == 'I') //ignore instruction cache accesses
        {
            continue;
        }
        else
        {
            sscanf(buf, " %c %lx, %d", &opreation, &address, &size);//important！！！ 空格
            switch (opreation)
            {
                case 'S':
                    ret = visitCache(address);
                    break;
                case 'L':
                    ret = visitCache(address);
                    break;
                case 'M':
                    ret = visitCache(address);
                    hits++;//store+load
                    break;
            }
            if(verbose)
            {
                switch (ret)
                {
                    case 0:
                        printf("%c %lx, %d hit\n", opreation, address, size);
                        break;
                    case 1:
                        printf("%c %lx, %d miss\n", opreation, address, size);
                        break;
                    case 2:
                        printf("%c %lx, %d miss, eviction\n", opreation, address, size);
                        break;
                }
            }
        }
    }
    for(int i = 0; i < S; i++)
        free(cache[i]);
    free(cache);
    fclose(fp);
    return 0;
}
/*return value
      0             cache hit
      1             cache miss
      2             cache miss, eviction
*/
int visitCache(uint64_t address)
{
    uint64_t tag = address >> (s+b);
    unsigned int setIndex = address>>b & ((1<<s) - 1);

    int empty = -1;
    int evict = 0;
    cacheSet cacheset = cache[setIndex];

    for(int i=0; i<E; i++)
    {
        if(cacheset[i].valid)
        {
            if(cacheset[i].tag == tag)
            {
                hits++;
                cacheset[i].lru = 1;
                return 0;
            }

            cacheset[i].lru++;
            if(cacheset[evict].lru <= cacheset[i].lru)
            {
                evict = i;
            }
        }
        else
        {
            empty = i;
        }
    }
    //for cache miss
    misses++;

    if(empty != -1)
    {
        cacheset[empty].valid = 1;
        cacheset[empty].tag = tag;
        cacheset[empty].lru = 1;
        return 1;//miss
    }
    else
    {
        cacheset[evict].tag = tag;
        cacheset[evict].lru = 1;
        evictions++;
        return 2;
    }
}





