#include "TEST_fs.h"
#include "constants.h"


struct file_system_type TEST_fs_type = {
	.owner = THIS_MODULE,
	.name = "TEST_fs",
	.mount = TEST_fs_mount,
	.kill_sb = TEST_fs_kill_superblock,	/* unmount */
};

const struct file_operations TEST_fs_file_ops = {
	.owner = THIS_MODULE,
	.llseek = generic_file_llseek,
	.mmap = generic_file_mmap,
	.fsync = generic_file_fsync,
	.read_iter = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
};

const struct file_operations TEST_fs_dir_ops = {
	.owner = THIS_MODULE,
	.iterate = TEST_fs_iterate,
};

const struct inode_operations TEST_fs_inode_ops = {
	.lookup = TEST_fs_lookup,
	.mkdir = TEST_fs_mkdir,
    .create = TEST_fs_create,
    .unlink = TEST_fs_unlink,
};

const struct super_operations TEST_fs_super_ops = {
    .evict_inode = TEST_evict_inode,
    .write_inode = TEST_write_inode,
};

const struct address_space_operations TEST_fs_aops = {
	.readpage = TEST_fs_readpage,
    .writepage = TEST_fs_writepage,
	.write_begin = TEST_fs_write_begin,
	.write_end = generic_write_end,
};

int save_super(struct super_block* sb)
{
    struct TEST_fs_super_block *disk_sb = sb->s_fs_info;
    struct buffer_head* bh;
    bh = sb_bread(sb, 1);
    printk(KERN_ERR "In save bmap\n");
    memcpy(bh->b_data, disk_sb, TEST_BLOCKSIZE);
    map_bh(bh, sb, 1);
    brelse(bh);
	return 0;
}

int TEST_fs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret = -EPERM;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);
	BUG_ON(!bh);
	struct TEST_fs_super_block *sb_disk;
	sb_disk = (struct TEST_fs_super_block *)bh->b_data;

	printk(KERN_INFO "TEST_fs: version num is %lu\n", sb_disk->version);
	printk(KERN_INFO "TEST_fs: magic num is %lu\n", sb_disk->magic);
	printk(KERN_INFO "TEST_fs: block_size num is %lu\n",
	       sb_disk->block_size);
	printk(KERN_INFO "TEST_fs: inodes_count num is %lu\n",
	       sb_disk->inodes_count);
	printk(KERN_INFO "TEST_fs: free_blocks num is %lu\n",
	       sb_disk->free_blocks);
	printk(KERN_INFO "TEST_fs: blocks_count num is %lu\n",
	       sb_disk->blocks_count);

	if (sb_disk->magic != MAGIC_NUM) {
		printk(KERN_ERR "Magic number not match!\n");
		goto release;
	}

	struct inode *root_inode;

	if (sb_disk->block_size != 4096) {
		printk(KERN_ERR "TEST_fs expects a blocksize of %d\n", 4096);
		ret = -EFAULT;
		goto release;
	}
	//fill vfs super block
	sb->s_magic = sb_disk->magic;
	sb->s_fs_info = sb_disk;
	sb->s_maxbytes = TEST_BLOCKSIZE * TEST_N_BLOCKS;	/* Max file size */
	sb->s_op = &TEST_fs_super_ops;

	//-----------test get inode-----
	struct TEST_inode raw_root_node;
	if (TEST_fs_get_inode(sb, TEST_ROOT_INODE_NUM, &raw_root_node) != -1) {
		printk(KERN_INFO "Get inode sucessfully!\n");
		printk(KERN_INFO "root blocks is %llu and block[0] is %llu\n",
		       raw_root_node.blocks, raw_root_node.block[0]);
	}
	//-----------end test-----------

	root_inode = new_inode(sb);
	if (!root_inode)
		return -ENOMEM;

	/* Our root inode. It doesn't contain useful information for now.
	 * Note that i_ino must not be 0, since valid inode numbers start at
	 * 1. */
	inode_init_owner(root_inode, NULL, S_IFDIR |
			 S_IRUSR | S_IXUSR |
			 S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	root_inode->i_sb = sb;
	root_inode->i_ino = TEST_ROOT_INODE_NUM;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
	    current_time(root_inode);
    
    root_inode->i_mode = raw_root_node.mode;
    root_inode->i_size = raw_root_node.dir_children_count;
	//root_inode->i_private = TEST_fs_get_inode(sb, TEST_ROOT_INODE_NUM);
	/* Doesn't really matter. Since this is a directory, it "should"
	 * have a link count of 2. See btrfs, though, where directories
	 * always have a link count of 1. That appears to work, even though
	 * it created a number of bug reports in other tools. :-) Just
	 * search the web for that topic. */
	inc_nlink(root_inode);

	/* What can you do with our inode? */
	root_inode->i_op = &TEST_fs_inode_ops;
	root_inode->i_fop = &TEST_fs_dir_ops;

	/* Make a struct dentry from our inode and store it in our
	 * superblock. */
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
		return -ENOMEM;
 release:
	brelse(bh);

	return 0;
}

struct dentry *TEST_fs_mount(struct file_system_type *fs_type, int flags,
			     const char *dev_name, void *data)
{
	struct dentry *ret;

	/* This is a generic mount function that accepts a callback. */
	ret = mount_bdev(fs_type, flags, dev_name, data, TEST_fs_fill_super);

	/* Use IS_ERR to find out if this pointer is a valid pointer to data
	 * or if it indicates an error condition. */
	if (IS_ERR(ret))
		printk(KERN_ERR "Error mounting TEST_fs\n");
	else
		printk(KERN_INFO "TEST_fs is succesfully mounted on [%s]\n",
		       dev_name);

	return ret;
}

void TEST_fs_kill_superblock(struct super_block *s)
{
	/* We don't do anything here, but it's important to call
	 * kill_block_super because it frees some internal resources. */
	kill_block_super(s);
	printk(KERN_INFO
	       "TEST_fs superblock is destroyed. Unmount succesful.\n");
}


/* Called when the module is loaded. */
int TEST_fs_init(void)
{
	int ret;

	ret = register_filesystem(&TEST_fs_type);
	if (ret == 0)
		printk(KERN_INFO "Sucessfully registered TEST_fs\n");
	else
		printk(KERN_ERR "Failed to register TEST_fs. Error: [%d]\n",
		       ret);

	return ret;
}

/* Called when the module is unloaded. */
void TEST_fs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&TEST_fs_type);

	if (ret == 0)
		printk(KERN_INFO "Sucessfully unregistered TEST_fs\n");
	else
		printk(KERN_ERR "Failed to unregister TEST_fs. Error: [%d]\n",
		       ret);
}

module_init(TEST_fs_init);
module_exit(TEST_fs_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("cv");
