#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#define ACPI_HWIN_SIG "HWIN"

struct hwinfo {
	struct acpi_table_header header;
	u32 cpu_temperature;
	u32 voltage;
	u32 secret;
};

static struct hwinfo *info;
static struct kobject *hwinfo_kobj;

static int __init hwinfo_init(void) {
    struct acpi_table_header *table;
	if (acpi_get_table(ACPI_HWIN_SIG, 1, &table) == AE_OK) {
		info = kmemdup(table, sizeof(struct hwinfo), GFP_KERNEL);
		acpi_put_table(table);
	}
	return 0;
}

static ssize_t cpu_temp_show(struct kobject *kobj,
			                 struct kobj_attribute *attr, char *buf) {
	if (!info) return -ENODATA;
	return scnprintf(buf, PAGE_SIZE, "%u\n", info->cpu_temperature);
}

static struct kobj_attribute cpu_temp_attr =
	__ATTR_RO(cpu_temp);

static ssize_t voltage_show(struct kobject *kobj,
			                struct kobj_attribute *attr, char *buf) {
	if (!info) return -ENODATA;
	return scnprintf(buf, PAGE_SIZE, "%u\n", info->voltage);
}

static struct kobj_attribute voltage_attr =
	__ATTR_RO(voltage);

static ssize_t secret_show(struct kobject *kobj,
			                struct kobj_attribute *attr, char *buf) {
	if (!info) return -ENODATA;
	return scnprintf(buf, PAGE_SIZE, "%u\n", info->secret);
}

static struct kobj_attribute secret_attr =
	__ATTR_RO(secret);

static struct attribute *hwin_attrs[] = {
	&cpu_temp_attr.attr,
	&voltage_attr.attr,
	&secret_attr.attr,
	NULL,
};

static struct attribute_group hwin_attr_group = {
	.attrs = hwin_attrs,
};

static int __init hwinfo_create(void)
{
	int ret;
	hwinfo_init();
	hwinfo_kobj = kobject_create_and_add("hwinfo", acpi_kobj ? acpi_kobj : firmware_kobj);
	if (!hwinfo_kobj)
		return -ENOMEM;
	ret = sysfs_create_group(hwinfo_kobj, &hwin_attr_group);
	if (ret)
		kobject_put(hwinfo_kobj);
	return 0;
}
subsys_initcall(hwinfo_create);