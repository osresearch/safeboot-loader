/** UEFI Block Device.
 *
 * Implements a simplistic block device that calls back
 * into the UEFI BlockDeviceProtocol.  This assumes that
 * the uefi memory map has already been setup
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include "efiwrapper.h"
#include "blockio.h"

static int debug = 0;
static int major;


typedef struct {
	spinlock_t lock;
	struct gendisk *gd;
	struct request_queue *queue;
	struct blk_mq_tag_set tag_set;
	atomic_t refcnt;

	EFI_BLOCK_IO_PROTOCOL *uefi_bio;
	uint8_t * buffer;
} uefi_blockdev_t;

static blk_status_t uefi_blockdev_request(struct blk_mq_hw_ctx * hctx, const struct blk_mq_queue_data * bd)
{
	struct request * rq = bd->rq;
	uefi_blockdev_t * dev = rq->rq_disk->private_data;
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
		// sector is *always* in Linux 512 blocks
		sector_t sector = iter.iter.bi_sector;
		size_t len = bvec.bv_len;
		char * buffer = kmap_atomic(bvec.bv_page);
		unsigned long offset = bvec.bv_offset;
		const bool is_write = bio_data_dir(iter.bio);
		const size_t bs = dev->uefi_bio->Media->BlockSize;
		const unsigned long disk_addr = sector * 512;
		const unsigned long disk_sector = disk_addr / bs;

		void * dest = buffer + offset;

		// how many full blocks do we have?
		size_t full_block_len = len & ~(bs - 1);
		size_t partial_block_len = len - full_block_len;

		// read and write have the same signature
		EFI_BLOCK_READ handler = is_write
			? dev->uefi_bio->WriteBlocks
			: dev->uefi_bio->ReadBlocks;

		if (debug)
		printk("%s.%d: %s %08llx %08lx %08lx => %08llx + %08llx @ %08llx + %08llx\n",
			dev->gd->disk_name,
			dev->uefi_bio->Media->MediaId,
			bio_data_dir(iter.bio) ? "WRITE" : "READ ",
			sector,
			disk_addr,
			disk_sector,
			(uint64_t) buffer,
			(uint64_t) offset,
			(uint64_t) full_block_len,
			(uint64_t) partial_block_len
		);

		// do an operation on as many full blocks as we can
		if (full_block_len != 0)
			status |= handler(
				dev->uefi_bio,
				dev->uefi_bio->Media->MediaId,
				disk_sector,
				full_block_len,
				dest
			);

		// and fix up any stragglers with our bounce buffer
		if (partial_block_len != 0)
		{
			const size_t full_blocks = full_block_len / bs;
			printk("tail %zu blocks + %zu bytes\n", full_blocks, partial_block_len);

			// pass through our block sized buffer
			if (is_write)
			{
				printk("%s: short write %llx\n", dev->gd->disk_name, (uint64_t) partial_block_len);
				memcpy(dev->buffer, dest, partial_block_len);
			}

			status |= handler(
				dev->uefi_bio,
				dev->uefi_bio->Media->MediaId,
				disk_sector + full_blocks,
				bs, // full block operation
				dev->buffer
			);

			if (!is_write)
			{
				// copy the desired amount out of our buffer
				memcpy(dest + full_block_len, dev->buffer, partial_block_len);
			}
		}

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

static struct blk_mq_ops uefi_blockdev_qops = {
	.queue_rq	= uefi_blockdev_request,
};

static int uefi_blockdev_open(struct block_device * bd, fmode_t mode)
{
	uefi_blockdev_t * const dev = bd->bd_disk->private_data;
	atomic_inc(&dev->refcnt);
	//printk("opened '%s'\n", bd->bd_disk->disk_name);
	return 0;
}

static void uefi_blockdev_release(struct gendisk * disk, fmode_t mode)
{
	uefi_blockdev_t * const dev = disk->private_data;
	atomic_dec(&dev->refcnt);
}

/* // todo: support removable media
static int uefi_blockdev_media_changed(struct gendisk * gd)
{
	uefi_blockdev_t * const dev = gd->private_data;
	return dev->uefi_bio->Media->MediaPresent;
}
*/

/* // todo: support ioctl
static int uefi_blockdev_ioctl(struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg)
{
	uefi_blockdev_t * const dev = inode->i_bdev->bd_disk->private_data;
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


static struct block_device_operations uefi_blockdev_fops = {
	.owner		= THIS_MODULE,
	.open		= uefi_blockdev_open,
	.release	= uefi_blockdev_release,
	// .ioctl		= uefi_blockdev_ioctl,
	// .media_changed	= uefi_blockdev_media_changed,
};


static void * uefi_blockdev_add(int minor, EFI_HANDLE handle, EFI_BLOCK_IO_PROTOCOL * uefi_bio)
{
	const EFI_BLOCK_IO_MEDIA * const media = uefi_bio->Media;
	struct gendisk * disk;
	uefi_blockdev_t * dev;
	void * fs;

	printk("uefi%d: %s\n", minor, uefi_device_path_to_name(handle));

	fs = uefi_handle_protocol(&EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, handle);
	printk("uefi%d: rev=%llx id=%d removable=%d present=%d logical=%d ro=%d caching=%d bs=%u size=%llu%s\n",
		minor,
		uefi_bio->Revision,
		media->MediaId,
		media->RemovableMedia,
		media->MediaPresent,
		media->LogicalPartition,
		media->ReadOnly,
		media->WriteCaching,
		media->BlockSize,
		media->LastBlock * media->BlockSize,
		fs ? " SIMPLE_FS" : "");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	if (media->BlockSize != 512)
	{
		printk("uefi%d: weird block size %d!\n", minor, media->BlockSize);
	}

	spin_lock_init(&dev->lock);
	atomic_set(&dev->refcnt, 0);
	dev->uefi_bio = uefi_bio;
	dev->buffer = kzalloc(media->BlockSize, GFP_KERNEL);

	//disk = dev->gd = blk_alloc_disk(0); // 5.15
	disk = dev->gd = alloc_disk(1); // 5.4
	if (!disk)
		return NULL;

	sprintf(disk->disk_name, "uefi%d", minor);

	disk->private_data	= dev;
	disk->major		= major;
	disk->first_minor	= minor;
	disk->minors		= 1;
	// todo: support removable media
	//disk->flags		= media->RemovableMedia ? GENHD_FL_REMOVABLE : 0;
	disk->fops		= &uefi_blockdev_fops;

	dev->tag_set.ops	= &uefi_blockdev_qops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = 128; // should be 1?
	dev->tag_set.numa_node	= NUMA_NO_NODE;
	dev->tag_set.cmd_size	= 0;
	dev->tag_set.flags	= BLK_MQ_F_SHOULD_MERGE;

	blk_mq_alloc_tag_set(&dev->tag_set);

	disk->queue = dev->queue = blk_mq_init_queue(&dev->tag_set);
	dev->queue->queuedata = dev;

	blk_queue_logical_block_size(dev->queue, media->BlockSize);
	set_capacity(disk, media->LastBlock * (media->BlockSize / 512)); // in Linux sectors


	add_disk(disk);

	return dev;
}

static int uefi_blockdev_handle_done(EFI_HANDLE handle)
{
	static EFI_HANDLE handles_done[64];
	static int handles_done_count;

	for(unsigned i = 0 ; i < handles_done_count ; i++)
	{
		if (handles_done[i] == handle)
			return 1;
	}

	// this one is new, add it to our lsit
	handles_done[handles_done_count++] = handle;
	return 0;
}

// called when there is a new block device driver registered
// it unfortunately finds all of them.
static void uefi_blockdev_scan(void * unused)
{
	EFI_HANDLE handles[64];
	int handle_count = uefi_locate_handles(&EFI_BLOCK_IO_PROTOCOL_GUID, handles, 64);
	int count = 0;

	if (handle_count < 1)
		return;

	for(unsigned i = 0 ; i < handle_count ; i++)
	{
		EFI_HANDLE handle = handles[i];
		EFI_BLOCK_IO_PROTOCOL * uefi_bio;

		// have we seen this one?
		if (uefi_blockdev_handle_done(handle))
			continue;
				
		uefi_bio = uefi_handle_protocol(
			&EFI_BLOCK_IO_PROTOCOL_GUID,
			handle
		);

		if (!uefi_bio)
			continue;

		uefi_blockdev_add(i, handle, uefi_bio);
		count++;
	}

	printk("uefi_blockdev: created %d block devices\n", count);
}


static int uefi_blockdev_register(void)
{
	uefi_register_protocol_callback(
		&EFI_BLOCK_IO_PROTOCOL_GUID,
		uefi_blockdev_scan,
		NULL
	);
	return 0;
}

int uefi_blockdev_init(void)
{
	major = register_blkdev(0, DRIVER_NAME);
	if (major < 0)
		return -EIO;

/*
	if (uefi_blockdev_scan() < 0)
		return -EIO;
*/

	// register the call back and initiate a scan
	if (uefi_blockdev_register() < 0)
		return -EIO;

	return 0;
}
