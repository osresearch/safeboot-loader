/** UEFI Block Device.
 *
 * Implements a simplistic block device that calls back
 * into the UEFI BlockDeviceProtocol.
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include "efiwrapper.h"
#include "blockio.h"

#define DRIVER_NAME	"uefiblockdev"
#define DRIVER_VERSION	"v0.1"
#define DRIVER_AUTHOR	"Trammell Hudson"
#define DRIVER_DESC	"UEFI Block Device driver"

static int debug = 0;
static int major;


typedef struct {
	spinlock_t lock;
	struct gendisk *gd;
	struct request_queue *queue;
	struct blk_mq_tag_set tag_set;
	atomic_t refcnt;

	EFI_BLOCK_IO_PROTOCOL *uefi_bio;
} uefiblockdev_t;

static blk_status_t uefiblockdev_request(struct blk_mq_hw_ctx * hctx, const struct blk_mq_queue_data * bd)
{
	struct request * rq = bd->rq;
	uefiblockdev_t * dev = rq->rq_disk->private_data;
	struct bio_vec bvec;
	struct req_iterator iter;
	int status = 0;

	uefi_memory_map_add();

	if (blk_rq_is_passthrough(rq))
	{
		printk(KERN_NOTICE "skip non-fs request\n");
		blk_mq_end_request(rq, BLK_STS_IOERR);
		return BLK_STS_OK;
	}

	rq_for_each_segment(bvec, rq, iter) {
		sector_t sector = iter.iter.bi_sector;
		size_t len = bvec.bv_len;
		char * buffer = kmap_atomic(bvec.bv_page);
		unsigned long offset = bvec.bv_offset;

		// read and write have the same signature
		EFI_BLOCK_READ handler = bio_data_dir(iter.bio)
			? dev->uefi_bio->WriteBlocks
			: dev->uefi_bio->ReadBlocks;

		if (debug)
		printk("%s.%d: %s %08llx => %08llx + %08llx @ %08llx\n",
			dev->gd->disk_name,
			dev->uefi_bio->Media->MediaId,
			bio_data_dir(iter.bio) ? "WRITE" : "READ ",
			sector,
			(uint64_t) buffer,
			(uint64_t) offset,
			(uint64_t) len
		);

		status |= handler(
			dev->uefi_bio,
			dev->uefi_bio->Media->MediaId,
			sector,
			len,
			buffer + offset
		);

		kunmap_atomic(buffer);
	}

/* // todo: figure out what replaced this
	// force writes if this is a barrier request
	if (blk_barrier_rq(rq))
		efi_call(dev->uefi_bio->FlushBlocks, dev->uefi_bio);
*/

	if (status)
		printk("%s: operation failed %x\n", dev->gd->disk_name, status);

	blk_mq_end_request(rq, status ? BLK_STS_IOERR : 0);
	return BLK_STS_OK;
}

static struct blk_mq_ops uefiblockdev_qops = {
	.queue_rq	= uefiblockdev_request,
};

static int uefiblockdev_open(struct block_device * bd, fmode_t mode)
{
	uefiblockdev_t * const dev = bd->bd_disk->private_data;
	atomic_inc(&dev->refcnt);
	//printk("opened '%s'\n", bd->bd_disk->disk_name);
	return 0;
}

static void uefiblockdev_release(struct gendisk * disk, fmode_t mode)
{
	uefiblockdev_t * const dev = disk->private_data;
	atomic_dec(&dev->refcnt);
}

/* // todo: support removable media
static int uefiblockdev_media_changed(struct gendisk * gd)
{
	uefiblockdev_t * const dev = gd->private_data;
	return dev->uefi_bio->Media->MediaPresent;
}
*/

/* // todo: support ioctl
static int uefiblockdev_ioctl(struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg)
{
	uefiblockdev_t * const dev = inode->i_bdev->bd_disk->private_data;
	struct hd_geometry geo = {
		.heads = 4,
		.sectors = 16,
		.start = 0,
		.cylinders = dev->uefi_bio->Media->LastBlock / 4 / 16,
	};

	switch(cmd) {
	case HDIO_GETGEO:
		if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
			return -EFAULT;
		return 0;
	}

	return -ENOTTY;
}
*/


static struct block_device_operations uefiblockdev_fops = {
	.owner		= THIS_MODULE,
	.open		= uefiblockdev_open,
	.release	= uefiblockdev_release,
	// .ioctl		= uefiblockdev_ioctl,
	// .media_changed	= uefiblockdev_media_changed,
};


static void * __init uefiblockdev_add(int minor, EFI_HANDLE handle, EFI_BLOCK_IO_PROTOCOL * uefi_bio)
{
	const EFI_BLOCK_IO_MEDIA * const media = uefi_bio->Media;
	struct gendisk * disk;
	uefiblockdev_t * dev;

	printk("uefi%d: %s\n", minor, uefi_device_path_to_name(handle));

	printk("uefi%d: rev=%llx id=%d removable=%d present=%d logical=%d ro=%d caching=%d bs=%u size=%llu\n",
		minor,
		uefi_bio->Revision,
		media->MediaId,
		media->RemovableMedia,
		media->MediaPresent,
		media->LogicalPartition,
		media->ReadOnly,
		media->WriteCaching,
		media->BlockSize,
		media->LastBlock * media->BlockSize);

	if (media->BlockSize != 512)
	{
		printk("uefi%d: block size %d unsupported!\n", minor, media->BlockSize);
		return NULL;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	spin_lock_init(&dev->lock);
	atomic_set(&dev->refcnt, 0);
	dev->uefi_bio = uefi_bio;
	disk = dev->gd = alloc_disk(1);
	if (!disk)
		return NULL;

	sprintf(disk->disk_name, "uefi%d", minor);

	disk->private_data	= dev;
	disk->major		= major;
	disk->first_minor	= minor;
	disk->minors		= 1;
	// todo: support removable media
	//disk->flags		= media->RemovableMedia ? GENHD_FL_REMOVABLE : 0;
	disk->fops		= &uefiblockdev_fops;

	dev->tag_set.ops	= &uefiblockdev_qops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = 128; // should be 1?
	dev->tag_set.numa_node	= NUMA_NO_NODE;
	dev->tag_set.cmd_size	= 0;
	dev->tag_set.flags	= BLK_MQ_F_SHOULD_MERGE;

	blk_mq_alloc_tag_set(&dev->tag_set);

	disk->queue = dev->queue = blk_mq_init_queue(&dev->tag_set);
	dev->queue->queuedata = dev;

	blk_queue_logical_block_size(dev->queue, media->BlockSize);
	set_capacity(disk, media->LastBlock); // in sectors


	add_disk(disk);

	return dev;
}

static int uefiblockdev_scan(void)
{
	EFI_HANDLE handles[64];
	int handle_count = uefi_locate_handles(&EFI_BLOCK_IO_PROTOCOL_GUID, handles, 64);
	int count = 0;

	printk("uefiblockdev: %d block devices\n", handle_count);
	if (handle_count < 1)
		return -1;

	for(unsigned i = 0 ; i < handle_count ; i++)
	{
		EFI_HANDLE handle = handles[i];
		EFI_BLOCK_IO_PROTOCOL * uefi_bio = uefi_handle_protocol(
			&EFI_BLOCK_IO_PROTOCOL_GUID,
			handle
		);

		if (!uefi_bio)
			continue;

		uefiblockdev_add(i, handle, uefi_bio);
		count++;
	}

	return count;
}

static int __init uefiblockdev_init(void)
{
	major = register_blkdev(0, DRIVER_NAME);
	if (major < 0)
		return -EIO;

	uefi_memory_map_add();

	if (uefiblockdev_scan() < 0)
		return -EIO;

	return 0;
}

module_init(uefiblockdev_init);

// todo: remove module support

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
