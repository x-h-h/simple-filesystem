#include "TEST_fs.h"
#include "constants.h"

int TEST_fs_readpage(struct file *file, struct page *page)
{
	printk(KERN_ERR "TEST: readpage");
	return block_read_full_page(page, TEST_fs_get_block);
}

int TEST_fs_writepage(struct page* page, struct writeback_control* wbc) {
	printk(KERN_ERR "TEST: in write page\n");
       return block_write_full_page(page, TEST_fs_get_block, wbc);
}

int TEST_fs_write_begin(struct file* file, struct address_space* mapping, 
		loff_t pos, unsigned len, unsigned flags, 
		struct page** pagep, void** fsdata) {
    int ret;
	printk(KERN_INFO "TEST: in write_begin\n");
    ret = block_write_begin(mapping, pos, len, flags, pagep, TEST_fs_get_block);
    if (unlikely(ret))
        printk(KERN_INFO "TEST: Write failed for pos [%llu], len [%u]\n", pos, len);
    return ret;
}


int TEST_fs_iterate(struct file *filp, struct dir_context *ctx)
{
	struct TEST_inode H_inode;
	struct super_block *sb = filp->f_inode->i_sb;

	printk(KERN_INFO "TEST_fs: Iterate on inode [%llu]\n",
	       filp->f_inode->i_ino);

	if (-1 == TEST_fs_get_inode(sb, filp->f_inode->i_ino, &H_inode))
		return -EFAULT;

	printk(KERN_INFO "H_inode.dir_children_count is %llu\n",
	       H_inode.dir_children_count);
	if (ctx->pos >= H_inode.dir_children_count) {
		return 0;
	}

	if (H_inode.blocks == 0) {
		printk(KERN_INFO
		       "TEST_fs: inode [%lu] has no data!\n",
		       filp->f_inode->i_ino);
		return 0;
	}

	uint64_t i, dir_unread;
	dir_unread = H_inode.dir_children_count;
	printk(KERN_INFO "TEST_fs: dir_unread [%llu]\n", dir_unread);
	if (dir_unread == 0) {
		return 0;
	}

	struct TEST_dir_record *dir_arr =
	    kmalloc(sizeof(struct TEST_dir_record) * dir_unread, GFP_KERNEL);

	struct buffer_head *bh;
	for (i = 0; (i < H_inode.blocks) && (dir_unread > 0); ++i) {
		bh = sb_bread(sb, H_inode.block[i]);
		uint64_t len = dir_unread * sizeof(struct TEST_dir_record);
		uint64_t off = H_inode.dir_children_count - dir_unread;
		if (len < TEST_BLOCKSIZE) {	//read over
			memcpy(dir_arr + (off * sizeof(struct TEST_dir_record)),
			       bh->b_data, len);
			dir_unread = 0;
		} else {
			memcpy(dir_arr + (off * sizeof(struct TEST_dir_record)),
			       bh->b_data, TEST_BLOCKSIZE);
			dir_unread -=
			    TEST_BLOCKSIZE / sizeof(struct TEST_dir_record);
		}
		brelse(bh);
	}
	for (i = 0; i < H_inode.dir_children_count; ++i) {
		printk(KERN_INFO " dir_arr[i].filename is %s\n",
		       dir_arr[i].filename);
		dir_emit(ctx, dir_arr[i].filename, strlen(dir_arr[i].filename),
			 dir_arr[i].inode_no, DT_REG);
		ctx->pos++;
	}
	kfree(dir_arr);
	printk(KERN_INFO "ctx->pos is %llu\n", ctx->pos);
	return 0;
}
