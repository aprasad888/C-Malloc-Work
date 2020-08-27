/*
 * CS 2110 Fall 2019
 * Author: Anusha Prasad
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this to print out stuff*/
#include <stdio.h>
/* we need this for the metadata_t struct and my_malloc_err enum definitions */
#include "my_malloc.h"
/* include this for any boolean methods */
#include <stdbool.h>

/*Function Headers
 * Here is a place to put all of your function headers
 * Remember to declare them as static
 */

/* Our freelist structure - our freelist is represented as a singly linked list
 * the freelist is sorted by address;
 */
metadata_t *address_list;

/* Set on every invocation of my_malloc()/my_free()/my_realloc()/
 * my_calloc() to indicate success or the type of failure. See
 * the definition of the my_malloc_err enum in my_malloc.h for details.
 * Similar to errno(3).
 */
enum my_malloc_err my_malloc_errno;

static void placeCanaryBlock(metadata_t *dataBlock) {
    unsigned long canBlock = ((uintptr_t) dataBlock ^ CANARY_MAGIC_NUMBER) + 1890;
    unsigned long *endBlock = (unsigned long *) ((uint8_t *)dataBlock + (dataBlock -> size) + TOTAL_METADATA_SIZE - sizeof(unsigned long));
    (dataBlock -> canary) = canBlock;
    *endBlock = canBlock;
}

static void addData(metadata_t *dataBlock) {
    if (address_list != NULL) {
        metadata_t *tempList = address_list;

        uint8_t *locat = ((uint8_t *)(dataBlock));
        uint8_t *currLocat = ((uint8_t *)(tempList));

        if ((uint8_t *) locat >= currLocat) {
            while ((tempList -> next) != NULL) {
                currLocat = ((uint8_t *)(tempList -> next));
                if ((uint8_t *) locat < currLocat) {
                    (dataBlock -> next) = (tempList -> next);
                    (tempList -> next) = dataBlock;
                    return;
                }
                tempList = (tempList -> next);
            }
        }
        if ((uint8_t *) locat < currLocat) {
            (dataBlock -> next) = tempList;
            address_list = dataBlock;
            return;
        }
        if ((tempList -> next) == NULL) {
            (tempList -> next) = dataBlock;
            return;
        }
    }
    address_list = dataBlock;
    return;
}

static void removeData(metadata_t *dataBlock) {
    if (address_list != dataBlock) {
        metadata_t *tempList = address_list;
        while ((tempList -> next) != NULL) {
            //the block is found
            if (dataBlock == (tempList -> next)) {
                (tempList -> next) = (dataBlock -> next);
                return;
            }
            tempList = (tempList -> next);
        }
        return;
    }
    address_list = (dataBlock -> next);
    (dataBlock -> next) = NULL;
    return;
}

static metadata_t *combData(metadata_t *dataBlock) {
    uint8_t *firstBlock = (uint8_t *)dataBlock;
    uint8_t *lastBlock = ((uint8_t *)dataBlock + (dataBlock -> size) + TOTAL_METADATA_SIZE); /******/
    uint8_t *rBlock = NULL;
    uint8_t *lBlock = NULL;
    metadata_t *tempList = address_list;

    if ((uint8_t *)(tempList) != lastBlock) {
        while (tempList != NULL) {
            rBlock = ((uint8_t *)(tempList -> next));
            lBlock = ((uint8_t *)tempList + (tempList -> size) + TOTAL_METADATA_SIZE);
            //checks
            if (rBlock == lastBlock) {
                (dataBlock -> size) = (dataBlock -> size) + TOTAL_METADATA_SIZE + ((tempList -> next) -> size);
                removeData((tempList -> next));
                removeData(dataBlock);
                addData(dataBlock);
                return dataBlock;
            }
            if (lBlock == firstBlock) {
                (tempList -> size) = (tempList -> size) + TOTAL_METADATA_SIZE + (dataBlock -> size);
                removeData(dataBlock);
                removeData(tempList);
                addData(tempList);
                return tempList;
            }
            tempList = (tempList -> next);
        }
        return NULL;
    }
    (dataBlock -> size) = (dataBlock -> size) + TOTAL_METADATA_SIZE + (tempList -> size);
    removeData(tempList);
    addData(dataBlock);
    return dataBlock;
}

//adds data block to the address list
static void *putList(metadata_t *dataBlock) {
    metadata_t *combBlock1 = combData(dataBlock);
    metadata_t *combBlock2 = combData(dataBlock);
    if (combBlock1 == NULL) {
        addData(dataBlock);
        return dataBlock;
    }
    if (combBlock2 == NULL) {
        return combBlock1;
    }
    return combBlock2;
}

/* MALLOC
 * See PDF for documentation
 */
void *my_malloc(size_t size) {
    //UNUSED_PARAMETER(size);
    //return (NULL);

    if (size != 0) {
        if ((SBRK_SIZE - TOTAL_METADATA_SIZE) >= size) {
            metadata_t *tempList = address_list;
            metadata_t *dataBlock = NULL;
            size_t newSize = TOTAL_METADATA_SIZE + size;
            int count1 = 0;
            while (!(tempList == NULL || count1 != 0)) {
                if ((tempList -> size) != size) {
                    tempList = (tempList -> next);
                } else {
                    dataBlock = tempList;
                    count1 = 1;
                }
            }
            //checks
            if (count1 == 0) {
                int count2 = 0;
                tempList = address_list;
                while (!(tempList == NULL || count2 != 0)) {
                    if ((tempList -> size) < (MIN_BLOCK_SIZE + size)) {
                        tempList = (tempList -> next);
                    } else {
                        dataBlock = tempList;
                        count2 = 1;
                    }
                }
            }
            if (dataBlock == NULL) {
                dataBlock = ((metadata_t *)(my_sbrk(SBRK_SIZE)));
                if (dataBlock != NULL) {
                    (dataBlock -> size) = (SBRK_SIZE - TOTAL_METADATA_SIZE);
                    (dataBlock -> next) = NULL;
                    placeCanaryBlock(dataBlock);
                    dataBlock = putList(dataBlock);
                } else {
                    my_malloc_errno = OUT_OF_MEMORY;
                    return NULL;
                }
            }
            my_malloc_errno = NO_ERROR;
            if ((dataBlock -> size) == size) {
                removeData(dataBlock);
                (dataBlock -> size) = size;
                (dataBlock -> next) = NULL;
                placeCanaryBlock(dataBlock);
                my_malloc_errno = NO_ERROR;
                return (dataBlock + 1);
            }
            if ((dataBlock -> size) > (MIN_BLOCK_SIZE + size)) {
                removeData(dataBlock);
                size_t lBlock = (dataBlock -> size) - newSize;
                (dataBlock -> size) = lBlock;
                putList(dataBlock);
                metadata_t *dataRet = ((metadata_t *)((uint8_t *)dataBlock + (dataBlock -> size) + TOTAL_METADATA_SIZE));
                (dataRet -> size) = size;
                placeCanaryBlock(dataRet);
                (dataRet -> next) = NULL;
                return (dataRet + 1);
            }
            return NULL;
        }
        my_malloc_errno = SINGLE_REQUEST_TOO_LARGE;
        return NULL;
    }
    my_malloc_errno = NO_ERROR;
    return NULL;
}

/* REALLOC
 * See PDF for documentation
 */
void *my_realloc(void *ptr, size_t size) {
    //UNUSED_PARAMETER(ptr);
    //UNUSED_PARAMETER(size);
    //return (NULL);
    if (ptr) {
        metadata_t *valPt = (metadata_t *) ptr - 1;
        unsigned long canBlock = ((uintptr_t) valPt ^ CANARY_MAGIC_NUMBER) + 1890;
        unsigned long *lastBlock = ((unsigned long *)((char *)valPt + (valPt -> size) + TOTAL_METADATA_SIZE - sizeof(unsigned long)));
        if ((((valPt -> canary) == canBlock) && (*lastBlock == canBlock)))  {
            metadata_t *temp = (metadata_t *)(my_malloc(size));
            size_t move = (valPt -> size);
            if (move > size) {
                move = size;
            }
            memcpy(temp, ptr, move);
            my_free(ptr);
            my_malloc_errno = NO_ERROR;
            return temp;
        }
        my_malloc_errno = CANARY_CORRUPTED;
        return NULL;
    }
    my_malloc_errno = NO_ERROR;
    return my_malloc(size);
}


/* CALLOC
 * See PDF for documentation
 */
void *my_calloc(size_t nmemb, size_t size) {
    //UNUSED_PARAMETER(nmemb);
    //UNUSED_PARAMETER(size);
    //return (NULL);
    metadata_t *dataBlock = my_malloc(nmemb * size);
    if (dataBlock) {
        int num = size * nmemb;
        memset(dataBlock, 0, num);
        my_malloc_errno = NO_ERROR;
        return dataBlock;
    }
     return NULL;
}

/* FREE
 * See PDF for documentation
 */
void my_free(void *ptr) {
    //UNUSED_PARAMETER(ptr);
    my_malloc_errno = NO_ERROR;

    if(ptr) {

        metadata_t *dataBlock = (metadata_t *)((char *) ptr - sizeof(metadata_t));
        unsigned long canBlock = ((uintptr_t) dataBlock ^ CANARY_MAGIC_NUMBER) + 1890;
        unsigned long *endBlock = (unsigned long *) ((char *)dataBlock + (dataBlock -> size) + TOTAL_METADATA_SIZE - sizeof(unsigned long));

        if (((dataBlock -> canary) == canBlock && *endBlock == canBlock)) {
            putList(dataBlock);
            return;
        }
        my_malloc_errno = CANARY_CORRUPTED;
        return;
    }
    return;
}

