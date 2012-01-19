/*
 *	w1_ds28ea00.c - w1 family (DS28EA00) driver
 *
 * Copyright (c) 2011 Mignanti Claudio <c.mignanti@gmail.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "w1.h"
#include "w1_int.h"
#include "w1_family.h"


#define W1_DS28EA00		0x42

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("w1 family 28 driver for DS28EA00 chip v2");


/* Here we can read serial number from the device and write pio status to it
 * simple usingin the single w1_slave attribute */

static ssize_t w1_ds28ea00_scratchpad(struct device *device,
	struct device_attribute *attr, char *out_buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct w1_master *dev = sl->master;
	u8 rbuf[9], crc, verdict;
	u8 wbuf[1] = { 0xBE };
	ssize_t ret = PAGE_SIZE;

	mutex_lock(&dev->mutex);
	if (!w1_reset_select_slave(sl)) {
		w1_write_block(dev, wbuf, 1);

		w1_read_block(dev, rbuf, 9);

		crc = w1_calc_crc8(rbuf, 8);
		if (rbuf[8] == crc)
			verdict = 1;

		ret -= snprintf(out_buf + PAGE_SIZE - ret, ret, "%02x %02x %02x %02x %02x %02x %02x %02x %02x : crc=%02x %s\n",
			rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7], rbuf[8], crc, (verdict) ? "YES" : "NO");

	} else {
		ret = 0;
	}
	mutex_unlock(&dev->mutex);
	return ret;
}

static ssize_t w1_ds28ea00_therm(struct device *device,
	struct device_attribute *attr, char *out_buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct w1_master *dev = sl->master;
	u8 rbuf[9], crc, verdict;
	u8 wbuf[2] = { 0x44, 0xBE };
	char p_int, p_dec;
	ssize_t ret = PAGE_SIZE;

	mutex_lock(&dev->mutex);
	if (!w1_reset_select_slave(sl)) {

		w1_write_block(dev, &wbuf[0], 1);	//conversione
		msleep(1000);				// wait

		w1_reset_select_slave(sl);
		w1_write_block(dev, &wbuf[1], 1);	//read scratchpad

		w1_read_block(dev, rbuf, 8);

		p_int = ((rbuf[1] & 0x7) << 4 | ((rbuf[0] >> 4) & 0xF));
		p_dec = rbuf[0] & 0xF;


		crc = w1_calc_crc8(rbuf, 8);
		if (rbuf[8] == crc)
			verdict = 1;
		
		ret -= snprintf(out_buf + PAGE_SIZE - ret, ret, "%02x %02x %02x %02x %02x %02x %02x %02x %02x : crc=%02x %s\n",
			rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7], rbuf[8], crc, (verdict) ? "YES" : "NO");
		
		ret -= snprintf(out_buf + PAGE_SIZE - ret, ret, "%02x %02x %02x %02x %02x %02x %02x %02x %02x t=%d.%d\n",
			rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7], rbuf[8], (int) p_int, p_dec);

	} else {
		ret = 0;
	}
	mutex_unlock(&dev->mutex);
	return ret;
}

static ssize_t w1_ds28ea00_pio_write(struct device *device,
	struct device_attribute *attr, char *in_buf, size_t count)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct w1_master *dev = sl->master;

	if (count < 1)
		return -1;

	u8 rbuf[8];
	u8 wbuf[4] = { 0xA5, 0xFF, 0xFF };

	if (count > 0) {
		wbuf[1] = wbuf[1] ^ in_buf[0];
	}

	wbuf[2] = ~wbuf[1];

	mutex_lock(&dev->mutex);
	if (!w1_reset_select_slave(sl)) {
		w1_write_block(dev, wbuf, 3);
		w1_read_block(dev, rbuf, 1);
	} else {
		return -1;
	}
	mutex_unlock(&dev->mutex);
	return count;
}

static ssize_t w1_ds28ea00_pio_read(struct device *device,
	struct device_attribute *attr, char *out_buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct w1_master *dev = sl->master;
	u8 rbuf[8];
	u8 wbuf[1] = { 0xF5 };
	int ret;

	mutex_lock(&dev->mutex);
	if (!w1_reset_select_slave(sl)) {
		w1_write_block(dev, wbuf, 1);
		w1_read_block(dev, rbuf, 1);
		w1_reset_select_slave(sl);

	} else {
		return -1;
	}
	mutex_unlock(&dev->mutex);

	ret = snprintf(out_buf, 30, "status=%X \n", rbuf[0]);

	return ret;
}

static struct device_attribute w1_ds28ea00_attr_scratchpad =
	__ATTR(scratchpad, S_IRUGO, w1_ds28ea00_scratchpad, NULL);
static struct device_attribute w1_ds28ea00_attr_usb_power =
	__ATTR(pio_value, S_IWUGO | S_IRUGO, w1_ds28ea00_pio_read, w1_ds28ea00_pio_write );
static struct device_attribute w1_ds28ea00_attr_therm =
	__ATTR(therm, S_IRUGO, w1_ds28ea00_therm, NULL);


static int w1_ds28ea00_add_slave(struct w1_slave *sl)
{
	device_create_file(&sl->dev, &w1_ds28ea00_attr_scratchpad);
	device_create_file(&sl->dev, &w1_ds28ea00_attr_usb_power);
	device_create_file(&sl->dev, &w1_ds28ea00_attr_therm);
	return 0;
}

static struct w1_family_ops w1_ds28ea00_fops = {
	.add_slave	= w1_ds28ea00_add_slave,
//	.remove_slave	= w1_ds28ea00_remove_slave,
};

static struct w1_family w1_ds28ea00_family = {
	.fid = W1_DS28EA00,
	.fops = &w1_ds28ea00_fops,
};

static int __init w1_ds28ea00_init(void)
{

	return w1_register_family(&w1_ds28ea00_family);
}

module_init(w1_ds28ea00_init);
