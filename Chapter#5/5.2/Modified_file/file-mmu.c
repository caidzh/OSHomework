/* file-mmu.c: ramfs MMU-based file operations
 *
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

/*
 * NOTE! This filesystem is probably most useful
 * not as a real filesystem, but as an example of
 * how virtual filesystems can be written.
 *
 * It doesn't get much simpler than this. Consider
 * that this file implements the full semantics of
 * a POSIX-compliant read-write filesystem.
 *
 * Note in particular how the filesystem does not
 * need to implement any data structures of its own
 * to keep track of the virtual data: using the VFS
 * caches is sufficient.
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/namei.h>
#include <linux/kernel.h>
#include <linux/uio.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "internal.h"

#define RAMFS_PERSIST_PATH "/persist_ramfs/"

static unsigned long ramfs_mmu_get_unmapped_area(struct file *file,
		unsigned long addr, unsigned long len, unsigned long pgoff,
		unsigned long flags)
{
	return current->mm->get_unmapped_area(file, addr, len, pgoff, flags);
}

static int ramfs_fsync(struct file *file, loff_t start, loff_t end, int datasync) {
	int ret = 0;
    char *buf = NULL;
    loff_t isize;
    char *persist_path = NULL;
	char tmp_name[NAME_MAX];
	char dst_name[NAME_MAX];
    struct file *persist_file = NULL;
	struct dentry *tmp_dentry = NULL, *dst_dentry = NULL;
	struct inode *dir_inode = NULL;
	struct path dir_path;
	int err;

    // ramfs no fsync

	// malloc buf
    isize = i_size_read(file_inode(file));
    if (isize <= 0)
        return 0;

    buf = kmalloc(isize, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    // file -> buf
	file->f_pos = 0;
    ret = kernel_read(file, buf, isize, &file->f_pos);
	if (ret < 0)
		goto out_free;

    // malloc temporary path
    persist_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!persist_path) {
        ret = -ENOMEM;
        goto out_free;
    }
    snprintf(persist_path, PATH_MAX, RAMFS_PERSIST_PATH "%pd.tmp", file->f_path.dentry);

    // open temporary file
    persist_file = filp_open(persist_path,O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (IS_ERR(persist_file)) {
        ret = PTR_ERR(persist_file);
        persist_file = NULL;
        goto out_free_path;
    }

    // buf -> temporary file
    ret = kernel_write(persist_file, buf, isize, &persist_file->f_pos);
	filp_close(persist_file, NULL);
	if (ret < 0)
		goto out_free_path;

    // rename temporary file to persistent file
	// guarantee atomic
	err = kern_path(RAMFS_PERSIST_PATH, LOOKUP_DIRECTORY, &dir_path);
	if (!err) {
		dir_inode = d_inode(dir_path.dentry);

		snprintf(tmp_name, NAME_MAX, "%pd.tmp", file->f_path.dentry);
		snprintf(dst_name, NAME_MAX, "%pd",    file->f_path.dentry);
		tmp_dentry = lookup_one_len(tmp_name, dir_path.dentry, strlen(tmp_name));
		dst_dentry = lookup_one_len(dst_name, dir_path.dentry, strlen(dst_name));
		if (!IS_ERR(tmp_dentry) && !IS_ERR(dst_dentry)) {
			struct renamedata rd = {
				.old_dir = dir_inode,
				.old_dentry = tmp_dentry,
				.new_dir = dir_inode,
				.new_dentry = dst_dentry,
				.delegated_inode = NULL,
				.flags = 0,
			};
			err = vfs_rename(&rd);
			dput(tmp_dentry);
			dput(dst_dentry);
		} else {
			if (!IS_ERR(tmp_dentry)) dput(tmp_dentry);
			if (!IS_ERR(dst_dentry)) dput(dst_dentry);
		}
		path_put(&dir_path);
		if (err)
			ret = err;
	} else {
		ret = err;
	}

out_free_path:
    kfree(persist_path);
out_free:
    kfree(buf);
    return ret;
}

const struct file_operations ramfs_file_operations = {
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	// .fsync		= noop_fsync,
	.fsync = ramfs_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.llseek		= generic_file_llseek,
	.get_unmapped_area	= ramfs_mmu_get_unmapped_area,
};

const struct inode_operations ramfs_file_inode_operations = {
	.setattr	= simple_setattr,
	.getattr	= simple_getattr,
};
