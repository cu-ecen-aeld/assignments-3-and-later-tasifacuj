/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>         // kmalloc()

#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("tasifacij");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev                 aesd_device;
struct aesd_circular_buffer     circular_buf;
static char*                    g_buff = NULL;
static size_t                   g_len = 0;

int aesd_open(struct inode *inode, struct file *filp){
    PDEBUG("open");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp){
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte_rtn;
    size_t bytes_read = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    if( mutex_lock_interruptible( &aesd_device.mtx ) ){
        return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos( &circular_buf, *f_pos, &entry_offset_byte_rtn );

    if( !entry ){
        mutex_unlock( &aesd_device.mtx );
        return 0;// no data.
    }

    while( entry && bytes_read < count ){
        size_t cpsz = entry->size - entry_offset_byte_rtn;

        if( cpsz > count - bytes_read ){
            cpsz = count - bytes_read;
        }

        if( copy_to_user( buf + bytes_read, entry->buffptr + entry_offset_byte_rtn, cpsz ) ){
            mutex_unlock( &aesd_device.mtx );
            return -EFAULT;
        }

        // go to next buffer chunk.
        bytes_read += cpsz;
        *f_pos += cpsz;
        entry_offset_byte_rtn = 0;
        entry = aesd_circular_buffer_find_entry_offset_for_fpos( &circular_buf, *f_pos, &entry_offset_byte_rtn );
    }

    mutex_unlock( &aesd_device.mtx );
    return bytes_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){
    size_t wr_len = 0;
    char* wbuff;
    size_t i;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    wbuff = kmalloc( count, GFP_KERNEL );

    if( !wbuff ){
        return -ENOMEM;
    }

    if( copy_from_user( wbuff, buf, count ) ){
        kfree( wbuff );
        return -EFAULT;
    }

    if( mutex_lock_interruptible( &aesd_device.mtx ) ){
        kfree( wbuff );
        return -ERESTARTSYS;
    }

    for( i = 0; i < count; i ++ ){
        char* b = krealloc( g_buff, g_len + 1, GFP_KERNEL );

        if( !b ){
            kfree( wbuff );
            mutex_unlock( &aesd_device.mtx );
            return -ENOMEM;
        }

        g_buff = b;
        g_buff[ g_len ] = wbuff[ i ];
        g_len ++;
        wr_len ++;

        if( wbuff[ i ] == '\n' ){
            struct aesd_buffer_entry e = {
                .buffptr = g_buff,
                .size = g_len
            };
            char const* old_buff = aesd_circular_buffer_add_entry( &circular_buf, &e );

            if( old_buff ){
                kfree( old_buff );
            }

            g_buff = NULL;
            g_len = 0;
        }
    }

    mutex_unlock( &aesd_device.mtx );
    kfree( wbuff );
    return wr_len;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }

    aesd_circular_buffer_init( &circular_buf );
    return result;
}

void aesd_cleanup_module(void){
    size_t i;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    for( i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i ++ ){
        if( circular_buf.entry[ i ].buffptr ){
            kfree( circular_buf.entry[ i ].buffptr );
            circular_buf.entry[ i ].buffptr = NULL;
        }
    }

    if( g_buff ){
        kfree( g_buff );
        g_buff = NULL;
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
