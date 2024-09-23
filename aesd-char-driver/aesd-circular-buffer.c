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
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer
    , size_t char_offset
    , size_t *entry_offset_byte_rtn )
{
    size_t total_bytes_size = 0;
    uint8_t index = buffer->out_offs;
    uint8_t count = 0;

    while (count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        struct aesd_buffer_entry *entry = &buffer->entry[index];

        if (entry->size == 0) {
            return NULL; // No more valid entries
        }

        if (total_bytes_size + entry->size > char_offset) {
            *entry_offset_byte_rtn = char_offset - total_bytes_size;
            return entry;
        }

        total_bytes_size += entry->size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        count++;

        if (index == buffer->in_offs && !buffer->full) {
            break; // Reached the end of valid entries
        }
    }

    return NULL; // char_offset not found
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
    struct aesd_buffer_entry * e = &( buffer->entry[ buffer->in_offs ] );

    if ( buffer->full ){
        old_buff = e->buffptr;
    }

    e->buffptr = add_entry->buffptr;
    e->size = add_entry->size;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    } else {
        buffer->full = false;
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