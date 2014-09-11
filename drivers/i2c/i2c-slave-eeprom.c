/*
 * I2C Slave mode EEPROM simulator
 *
 * Copyright (C) 2014 by Wolfram Sang, Sang Engineering <wsa@sang-engineering.com>
 * Copyright (C) 2014 by Renesas Solutions Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

//TODO: simulate 24c32 as well
#define EEPROM_SIZE 256

struct at24s_data {
	//TODO: use lock
	struct mutex lock;
	struct bin_attribute bin;
	u8 buffer[EEPROM_SIZE];
	u8 eeprom_ptr;
	bool first_write;
};

static int at24s_slave_cb(struct i2c_client *client, enum i2c_slave_event event, u8 *val)
{
	struct at24s_data *at24s = i2c_get_clientdata(client);

	switch (event) {
	case I2C_SLAVE_REQ_WRITE_END:
		if (at24s->first_write) {
			at24s->eeprom_ptr = *val;
			at24s->first_write = false;
		} else {
			at24s->buffer[at24s->eeprom_ptr++] = *val;
		}
		break;

	case I2C_SLAVE_REQ_READ_START:
		*val = at24s->buffer[at24s->eeprom_ptr];
		break;

	case I2C_SLAVE_REQ_READ_END:
		at24s->eeprom_ptr++;
		break;

	case I2C_SLAVE_STOP:
		at24s->first_write = true;
		break;

	default:
		break;
	}

	return 0;
}

static ssize_t at24s_bin_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct at24s_data *at24s;

	at24s = dev_get_drvdata(container_of(kobj, struct device, kobj));

	//TODO: boundary checks
	memcpy(buf, &at24s->buffer[off], count);

	return count;
}

static ssize_t at24s_bin_write(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct at24s_data *at24s;

	if (unlikely(off >= attr->size))
		return -EFBIG;

	at24s = dev_get_drvdata(container_of(kobj, struct device, kobj));

	//TODO: boundary checks
	memcpy(&at24s->buffer[off], buf, count);

	return count;
}

static int i2c_slave_eeprom_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct at24s_data *at24s;
	int ret;

	at24s = devm_kzalloc(&client->dev, sizeof(struct at24s_data), GFP_KERNEL);
	if (!at24s)
		return -ENOMEM;

	at24s->first_write = true;
	i2c_set_clientdata(client, at24s);

	ret = i2c_slave_register(client, at24s_slave_cb);
	if (ret)
		return ret;

	sysfs_bin_attr_init(&at24s->bin);
	at24s->bin.attr.name = "slave-eeprom";
	at24s->bin.attr.mode = S_IRUSR | S_IWUSR;
	at24s->bin.read = at24s_bin_read;
	at24s->bin.write = at24s_bin_write;
	at24s->bin.size = EEPROM_SIZE;

	ret = sysfs_create_bin_file(&client->dev.kobj, &at24s->bin);
	if (ret)
		return ret;

	return 0;
};

static int i2c_slave_eeprom_remove(struct i2c_client *client)
{
	struct at24s_data *at24s;

	at24s = i2c_get_clientdata(client);
	sysfs_remove_bin_file(&client->dev.kobj, &at24s->bin);

	return i2c_slave_unregister(client);
}

static const struct i2c_device_id i2c_slave_eeprom_id[] = {
	{ "slave-24c02", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_slave_eeprom_id);

static struct i2c_driver i2c_slave_eeprom_driver = {
	.driver = {
		.name = "i2c-slave-eeprom",
		.owner = THIS_MODULE,
	},
	.probe = i2c_slave_eeprom_probe,
	.remove = i2c_slave_eeprom_remove,
	.id_table = i2c_slave_eeprom_id,
};
module_i2c_driver(i2c_slave_eeprom_driver);

MODULE_AUTHOR("Wolfram Sang <wsa@sang-engineering.com>");
MODULE_DESCRIPTION("I2C slave mode EEPROM simulator");
MODULE_LICENSE("GPL v2");
