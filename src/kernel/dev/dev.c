/*
 * Copyright(C) 2011-2017 Pedro H. Penna   <pedrohenriquepenna@gmail.com>
 *              2016-2016 Subhra S. Sarkar <rurtle.coder@gmail.com>
 *              2017-2017 Clement Rouquier <clementrouquier@gmail.com>
 * 
 * This file is part of Nanvix.
 * 
 * Nanvix is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Nanvix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Nanvix. If not, see <http://www.gnu.org/licenses/>.
 */

#include <dev/ata.h>
#include <dev/klog.h>
#include <dev/tty.h>
#include <dev/cmos.h>
#include <dev/ramdisk.h>
#include <dev/8250.h>
#include <nanvix/const.h>
#include <nanvix/dev.h>
#include <nanvix/klib.h>
#include <nanvix/pm.h>
#include <nanvix/clock.h>
#include <nanvix/debug.h>
#include <errno.h>

/*============================================================================*
 *                            Character Devices                               *
 *============================================================================*/

/* Number of character devices. */
#define NR_CHRDEV 3

/*
 * Character devices table.
 */
PRIVATE const struct cdev *cdevsw[NR_CHRDEV] = {
	NULL, /* /dev/null */
	NULL, /* /dev/tty  */
	NULL  /* /dev/klog */
};

/**
 * @brief Registers a character device.
 * 
 * @details Attempts to register a character device with the major number @p
 *          major.
 * 
 * @returns Upon successful completion, zero is returned. Upon failure, a
 *          negative error code is returned instead.
 */
PUBLIC int cdev_register(unsigned major, const struct cdev *dev)
{
	/* Invalid major number? */
	if (major >= NR_CHRDEV)
		return (-EINVAL);
	
	/* Device already registered? */
	if ((major == NULL_MAJOR) || (cdevsw[major] != NULL))
		return (-EBUSY);
	
	/* Register character device. */
	cdevsw[major] = dev;

	return (0);
}

/**
 * @brief Writes bytes to a character device.
 * 
 * @details Attempts to write @p n bytes from the buffer pointed to by @p buf
 *          to the character device identified by @p dev.
 * 
 * @returns The number of bytes actually written to the device. Upon failure, a
 *          negative error code is returned.
 */
PUBLIC ssize_t cdev_write(dev_t dev, const void *buf, size_t n)
{	
	/* Null device. */
	if (MAJOR(dev) == NULL_MAJOR)
		return ((ssize_t)n);
	
	/* Invalid device. */
	if (cdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (cdevsw[MAJOR(dev)]->write == NULL)
		return (-ENOTSUP);
	
	return (cdevsw[MAJOR(dev)]->write(MINOR(dev), buf, n));
}

/*
 * @brief Reads a character from a character device.
 *
 * @details Attempts to read @p n bytes from the character device identified
 * by @p dev to the buffer pointed to by @p buf.
 * 
 * @returns Upon successful completion, the number of bytes actually read
 * from the device is returned. Upon failure, a negative error code is
 * returned.
 */
PUBLIC ssize_t cdev_read(dev_t dev, void *buf, size_t n)
{
	/* Null device. */
	if (MAJOR(dev) == NULL_MAJOR)
		return (0);
	
	/* Invalid device. */
	if (cdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (cdevsw[MAJOR(dev)]->read == NULL)
		return (-ENOTSUP);
	
	return (cdevsw[MAJOR(dev)]->read(MINOR(dev), buf, n));
}

/*
 * Opens a character device.
 */
PUBLIC int cdev_open(dev_t dev)
{
	/* Null device. */
	if (MAJOR(dev) == NULL_MAJOR)
		return (0);
	
	/* Invalid device. */
	if (cdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (cdevsw[MAJOR(dev)]->open == NULL)
		return (-ENOTSUP);
		
	return (cdevsw[MAJOR(dev)]->open(MINOR(dev)));
}

/*
 * Performs control operations on a character device.
 */
PUBLIC int cdev_ioctl(dev_t dev, unsigned cmd, unsigned arg)
{	
	/* Null device. */
	if (MAJOR(dev) == NULL_MAJOR)
		return (-ENODEV);
	
	/* Invalid device. */
	if (cdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (cdevsw[MAJOR(dev)]->ioctl == NULL)
		return (-ENOTSUP);
		
	return (cdevsw[MAJOR(dev)]->ioctl(MINOR(dev), cmd, arg));
}

/**
 * @brief Closes a character device.
 * 
 * @details Closes the character device @p dev.
 * 
 * @returns Upon successful completion zero is returned. Upon failure, a 
 *          negative error code is returned instead.
 */
PUBLIC int cdev_close(dev_t dev)
{
	/* Null device. */
	if (MAJOR(dev) == NULL_MAJOR)
		return (0);
		
	/* Invalid device. */
	if (cdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (cdevsw[MAJOR(dev)]->close == NULL)
		return (-ENOTSUP);
		
	return (cdevsw[MAJOR(dev)]->close(MINOR(dev)));
}


/**
 * @brief Tests if all character devices are correctly registered.
 * 
 * @returns Upon successful completion non-zero is returned. Upon failure, a
 * zero is returned instead.
 */
PRIVATE int cdevtst_register(void)
{
	for (int i = 0; i < NR_CHRDEV; i++)
	{
		if ((cdevsw[i] == NULL) && (i > 0))
		{
			kprintf(KERN_DEBUG "cdev test: register of device number %d failed", i);
			return 0;
		}
	}

	return (1);
}

/**
 * @brief Tests if cdev_write works correctly.
 * 
 * @param buffer         Buffer to be written in the character device.
 * @param tstcdev_lenght Number of characters to be written in the character device.
 * 
 * @returns Upon successful completion non-zero is returned. Upon failure, a
 * zero is returned instead.
 */
PRIVATE int cdevtst_w(char *buffer, int tstcdev_lenght)
{
	int char_count;

	char_count = cdev_write(2, buffer, tstcdev_lenght);

	if (char_count != tstcdev_lenght)
	{
		if(char_count <= 0)
			kprintf(KERN_DEBUG "cdev test: cdev_write failed: nothing has been written, code: %d",char_count);
		else
			kprintf(KERN_DEBUG "cdev test: cdev_write failed: what has been written is not what it has to be write, code: %d",char_count);

		return (0);
	}

	return (1);
}

/**
 * @brief Unit test for character devices.
 */
PUBLIC void cdev_test(void)
{
	char buffer[KBUFFER_SIZE]; /* Temporary buffer.                    */
	int tstcdev_lenght = 35;   /* Size of message to write in the log. */

	kstrncpy(buffer, "cdev test: test data input in cdev\n", tstcdev_lenght);

	if (!cdevtst_register())
	{
		tst_failed();
		return;
	}

	if (!cdevtst_w(buffer, tstcdev_lenght))
	{
		tst_failed();
		return;
	}

	tst_passed();
}


/*============================================================================*
 *                              Block Devices                                 *
 *============================================================================*/

/* Number of block devices. */
#define NR_BLKDEV 2

/*
 * Block devices table.
 */
PRIVATE const struct bdev *bdevsw[NR_BLKDEV] = {
	NULL, /* /dev/ramdisk */
	NULL  /* /dev/hdd     */
};

/**
 * @brief Registers a block device.
 *
 * @details Attempts to register a block device @p bdev with major number @p
 * major.
 *
 * @returns Upon successful completion, zero is returned. Upon failure, a
 * negative error code is returned.
 */
PUBLIC int bdev_register(unsigned major, const struct bdev *dev)
{
	/* Invalid major number? */
	if (major >= NR_BLKDEV)
		return (-EINVAL);
	
	/* Device already registered? */
	if (bdevsw[major] != NULL)
		return (-EBUSY);
	
	/* Register block device. */
	bdevsw[major] = dev;
	
	return (0);
}

/**
 * @brief Writes bytes to a block device.
 *
 * @returns Attempts to write @p n bytes from the buffer pointed to by @p
 * buf to the block device identified by @p dev, starting at offset @p off.
 * 
 * @returns Upon successful completion, the number of bytes that were
 * actually written to the device is returned. Upon failure, a negative
 * error code is returned.
 */
PUBLIC ssize_t bdev_write(dev_t dev, const char *buf, size_t n, off_t off)
{
	/* Invalid device. */
	if (bdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
	
	/* Operation not supported. */
	if (bdevsw[MAJOR(dev)]->write == NULL)
		return (-ENOTSUP);
	
	return (bdevsw[MAJOR(dev)]->write(MINOR(dev), buf, n, off));
}

/**
 * @brief Reads bytes from a block device.
 *
 * @details Attempts to read @p n bytes from the block device identified by
 * @p dev to the buffer pointed to by @p buf, starting at offset @p off.
 * 
 * @returns Upon successful completion, returns the number of bytes that
 * were actually read from the device. Upon failure, a negative error code
 * is returned.
 */
PUBLIC ssize_t bdev_read(dev_t dev, char *buf, size_t n, off_t off)
{
	/* Invalid device. */
	if (bdevsw[MAJOR(dev)] == NULL)
		return (-EINVAL);
		
	/* Operation not supported. */
	if (bdevsw[MAJOR(dev)]->read == NULL)
		return (-ENOTSUP);
	
	return (bdevsw[MAJOR(dev)]->read(MINOR(dev), buf, n, off));
}

/*
 * Writes a block to a block device.
 */
PUBLIC void bdev_writeblk(buffer_t buf)
{
	int err;   /* Error ?        */
	dev_t dev; /* Device number. */
	
	dev = buffer_dev(buf);
	
	/* Invalid device. */
	if (bdevsw[MAJOR(dev)] == NULL)
		kpanic("writing block to invalid device");
		
	/* Operation not supported. */
	if (bdevsw[MAJOR(dev)]->writeblk == NULL)
		kpanic("block device cannot write blocks");
		
	/* Write block. */
	err = bdevsw[MAJOR(dev)]->writeblk(MINOR(dev), buf);
	if (err)
		kpanic("failed to write block to device");
}

/*
 * Reads a block from a block device.
 */
PUBLIC void bdev_readblk(buffer_t buf)
{
	int err;   /* Error ?        */
	dev_t dev; /* Device number. */
	
	dev = buffer_dev(buf);
	
	/* Invalid device. */
	if (bdevsw[MAJOR(dev)] == NULL)
		kpanic("reading block from invalid device");
		
	/* Operation not supported. */
	if (bdevsw[MAJOR(dev)]->readblk == NULL)
		kpanic("block device cannot read blocks");
	
	/* Read block. */
	err = bdevsw[MAJOR(dev)]->readblk(MINOR(dev), buf);
	if (err)
		kpanic("failed to read block from device");
}

/**
 * @brief Tests if all block devices are correctly registered.
 * 
 * @returns Upon successful completion non-zero is returned. Upon failure, a 
 *          zero is returned instead.
 */
PRIVATE int bdevtst_register(void)
{
	for (int i = 0; i < NR_BLKDEV; i++)
	{
		 if (bdevsw[i] == NULL)
		{
			if (i == ATA_MAJOR)
			{
				kprintf(KERN_DEBUG "bdev test: warning: ATA device was not registered during initialization");
				continue;
			}
			else
			{
				kprintf(KERN_DEBUG "bdev test: register of device number %d failed", i);
				return (0);
			}
		} 
	}

	return (1);
}


/**
 * @brief Tests if bdev_write works correctly.
 * 
 * @param buffer         Buffer to be written in the block device.
 * @param tstbdev_lenght Number of characters to be written in the block device.
 * 
 * @returns Upon successful completion non-zero is returned. Upon failure, a
 * zero is returned instead.
 */
PRIVATE int bdevtst_w(char *buffer, int tstbdev_lenght)
{
	int char_count;

	char_count = bdev_write(2, buffer, tstbdev_lenght, 0);

	if (char_count != tstbdev_lenght)
	{
		if (char_count <= 0)
			kprintf(KERN_DEBUG "bdev test: bdev_write failed: nothing has been written, code: %d",char_count);
		else
			kprintf(KERN_DEBUG "bdev test: bdev_write failed: what has been written is not what it has to be write, code: %d",char_count);

		return (0);
	}

	return (1);
}

/**
 * @brief Unit test for block character devices.
 */
PUBLIC void bdev_test(void)
{
	int tstbdev_lenght = 35;   /* Size of message to write in the log */
	char buffer[KBUFFER_SIZE]; /* Temporary buffer.                   */

	kstrncpy(buffer, "bdev test: test data input in bdev\n", tstbdev_lenght);

	if (!bdevtst_register())
	{
		tst_failed();
		return;
	}

	if (!bdevtst_w(buffer, tstbdev_lenght))
	{
		tst_failed();
		return;
	}

	tst_passed();
}

/*============================================================================*
 *                                 Devices                                    *
 *============================================================================*/


/*
 * @brief Initializes the device drivers.
 */
PUBLIC void dev_init(void)
{
	klog_init();
	cmos_init();
	clock_init(CLOCK_FREQ);
	tty_init();
	ramdisk_init();
	uart8250_init();
	dbg_register(cdev_test, "cdev_test");
	dbg_register(bdev_test, "bdev_test");
}
