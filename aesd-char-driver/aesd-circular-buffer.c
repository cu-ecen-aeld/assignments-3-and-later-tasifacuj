/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn ){
    size_t off = buffer->out_offs;// start from ZERO position, because it looks like char_offset starts from ZERO,  cannot move out_ofs.
    size_t idx = off % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    struct aesd_buffer_entry* e = &( buffer->entry[ idx ] );

    while( off < buffer->in_offs && char_offset >= e->size ){
        char_offset -= e->size;
        off ++;
        idx = off % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        e = &( buffer->entry[ idx ] );
    }

    if( off == buffer->in_offs ){
        return NULL;
    }

    *entry_offset_byte_rtn = char_offset;
    return e;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    char const* old_buff = NULL;

    size_t idx = buffer->in_offs % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    struct aesd_buffer_entry * e = &( buffer->entry[ idx ] );    

    if( buffer->out_offs == buffer->in_offs - AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ){
        buffer->full = true;
    }else{
        buffer->full = false;
    }

    if( buffer->full ){
        old_buff = e->buffptr;
    }

    e->buffptr = add_entry->buffptr;
    e->size = add_entry->size;

    buffer->in_offs ++;

    if( buffer->full ){
        buffer->out_offs ++;
    }

    return old_buff;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

size_t aesd_circular_buffer_size( struct aesd_circular_buffer *buffer ){
    size_t sz = 0;
    int i = 0;

    for( ; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i ++ ){
        struct aesd_buffer_entry* e = &( buffer->entry[ i ] );

        if( e->buffptr ){
            sz += e->size;
        }else{
            break;
        }
    }

    return sz;
}

size_t aesd_circullar_buffer_size( struct aesd_circular_buffer* buffer ){
    size_t sz = 0;
    int i = 0;

    for( ; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i ++ ){
        struct aesd_buffer_entry* e = &( buffer->entry[ i ] );

        if( e->buffptr ){
            sz ++;
        }else{
            break;
        }
    }

    return sz;
}

size_t aesd_circular_buffer_seek( struct aesd_circular_buffer* buffer, uint32_t command ){
    size_t off = 0;
    int i = 0;

    if( command >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ){
        return off;
    }

    for( ; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i ++ ){
        struct aesd_buffer_entry* e = &( buffer->entry[ i ] );

        if( !e->buffptr || i == command ){
            break;
        }else{
            off += e->size;
        }
    }

    return off;
}