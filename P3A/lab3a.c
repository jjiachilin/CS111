#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "ext2_fs.h"

#define SUPERBLOCK_OFFSET 1024

int fd, num_groups;
__u32 block_size;
struct ext2_super_block super;

int offset(int block_num);
void gmt_time(__u32 time, char* buf);
void superblock();
void group(int group_num);
void free_blocks(int group_num, int bitmap);
void inodes(int group_num, int bitmap_block_num);
void allocated_inodes(int group_num, int bitmap_block_num, int inode_table_num);
void directory(int inode_num, int dir_block_num);
void indirect(int level, int inode_num, int block_num, char filetype);

int offset(int block_num)
{
    return SUPERBLOCK_OFFSET + (block_num - 1) * block_size;
}

void gmt_time(__u32 time, char* buf)
{
    time_t epoch = time;
    struct tm* t = gmtime(&epoch);
    if (!t)
    {
        fprintf(stderr, "Error getting gmtime: %s\n", strerror(errno));
        exit(1);
    }
    strftime(buf, 80, "%m/%d/%y %H:%M:%S", t);
}

// superblock summary
void superblock() 
{
    pread(fd, &super, sizeof(super), SUPERBLOCK_OFFSET);
    block_size = 1024 << super.s_log_block_size;
    printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", 
        super.s_blocks_count, 
        super.s_inodes_count,
        block_size,
        super.s_inode_size,
        super.s_blocks_per_group,
        super.s_inodes_per_group,
        super.s_first_ino
        );
}

// group summary
void group(int group_num) 
{
    struct ext2_group_desc gd;
    int group_desc_block;
    if (block_size == 1024) group_desc_block = 2;
    else if (block_size > 1024) group_desc_block = 1;

    int group_offset = block_size * group_desc_block + sizeof(struct ext2_group_desc) * group_num;
    pread(fd, &gd, sizeof(gd), group_offset);

    // last block has residue
    __u32 blocks_in_group = super.s_blocks_per_group;
    __u32 inodes_in_group = super.s_inodes_per_group;
    if (group_num == num_groups - 1)
    {
        blocks_in_group = super.s_blocks_count - (super.s_blocks_per_group * (num_groups - 1));
        inodes_in_group = super.s_inodes_count - (super.s_inodes_per_group * (num_groups - 1));
    }

    printf("GROUP,%d,%u,%u,%u,%u,%u,%u,%u\n",
        group_num,
        blocks_in_group,
        inodes_in_group,
        gd.bg_free_blocks_count,
        gd.bg_free_inodes_count,
        gd.bg_block_bitmap,
        gd.bg_inode_bitmap,
        gd.bg_inode_table
        );

    free_blocks(group_num, (int) gd.bg_block_bitmap);
    inodes(group_num, gd.bg_inode_bitmap);
    allocated_inodes(group_num, gd.bg_inode_bitmap, gd.bg_inode_table);
}

// free block entries
void free_blocks(int group_num, int bitmap_block_num)
{
    char* bitmap = (char*) malloc(block_size); 
    int bitmap_offset = offset(bitmap_block_num);
    int free_block_num = super.s_first_data_block + group_num * super.s_blocks_per_group;
    pread(fd, bitmap, block_size, bitmap_offset);

    unsigned int i, j;
    for (i = 0; i < block_size; ++i) 
    {
        char bit = bitmap[i];
        for (j = 0; j < 8; ++j) 
        {
            if (!(1 & bit))
                printf("BFREE,%d\n", free_block_num);
            bit >>= 1;
            ++free_block_num;
        }
    }
    free(bitmap);
}

// free I-node entries
void inodes(int group_num, int bitmap_block_num)
{
    int num_bytes = super.s_inodes_per_group / 8;
    char* bitmap = (char*) malloc(num_bytes);
    int bitmap_offset = offset(bitmap_block_num);
    int inode_num = group_num * super.s_inodes_per_group + 1;
    int index = 0;
    pread(fd, bitmap, num_bytes, bitmap_offset);

    int i, j;
    for (i = 0; i < num_bytes; ++i) 
    {
        char bit = bitmap[i];
        for (j = 0; j < 8; ++j) 
        {
            if (!(1 & bit))
                printf("IFREE,%d\n", inode_num);
            bit >>= 1;
            ++inode_num;
            ++index;
        }
    }
    free(bitmap);
}

// I-node summary
void allocated_inodes(int group_num, int bitmap_block_num, int inode_table_num)
{
    int num_bytes = super.s_inodes_per_group / 8;
    char* bitmap = (char*) malloc(num_bytes);
    int bitmap_offset = offset(bitmap_block_num);
    int inode_num = group_num * super.s_inodes_per_group + 1;
    int index = 0;
    pread(fd, bitmap, num_bytes, bitmap_offset);

    int i, j;
    for (i = 0; i < num_bytes; ++i) 
    {
        char bit = bitmap[i];
        for (j = 0; j < 8; ++j) 
        {
            if ((1 & bit))
            {
                struct ext2_inode inode;
                int inode_offset = offset(inode_table_num) + index * sizeof(inode);
                pread(fd, &inode, sizeof(inode), inode_offset);
    
                if (inode.i_mode != 0 && inode.i_links_count != 0) 
                {
                    char filetype = '?';
                    if (S_ISDIR(inode.i_mode)) filetype = 'd';
                    else if (S_ISREG(inode.i_mode)) filetype = 'f';
                    else if (S_ISLNK(inode.i_mode)) filetype = 's';

                    char ctime[30], mtime[30], atime[30];
                    gmt_time(inode.i_ctime, ctime);
                    gmt_time(inode.i_mtime, mtime);
                    gmt_time(inode.i_atime, atime);
                
                    printf("INODE,%d,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u",
                        inode_num,
                        filetype,
                        inode.i_mode & 0xFFF,
                        inode.i_uid,
                        inode.i_gid,
                        inode.i_links_count,
                        ctime,
                        mtime,
                        atime,
                        inode.i_size,
                        inode.i_blocks
                    );
                    
                    int i;
                    if (filetype != 's' || inode.i_size >= 60) // if file length is less than 60, don't print 15 blocks
                        for (i = 0; i < 15; ++i)
                            printf(",%u", inode.i_block[i]);
                    printf("\n");

                    for (i = 0; i < EXT2_NDIR_BLOCKS; ++i)
                        if (inode.i_block[i] != 0 && filetype == 'd')
                            directory(inode_num, inode.i_block[i]);
                    if (inode.i_block[EXT2_IND_BLOCK] != 0)
                        indirect(1, inode_num, inode.i_block[EXT2_IND_BLOCK], filetype);
                    if (inode.i_block[EXT2_DIND_BLOCK] != 0)
                        indirect(2, inode_num, inode.i_block[EXT2_DIND_BLOCK], filetype);
                    if (inode.i_block[EXT2_TIND_BLOCK] != 0)
                        indirect(3, inode_num, inode.i_block[EXT2_TIND_BLOCK], filetype);
                }
            }
            bit >>= 1;
            ++inode_num;
            ++index;
        }
    }
    free(bitmap);
}

// directory entries
void directory(int inode_num, int dir_block_num)
{
    struct ext2_dir_entry dir;
    int dir_offset = offset(dir_block_num);
    unsigned int logical_offset = 0;
    while (logical_offset < block_size)
    {
        pread(fd, &dir, sizeof(dir), dir_offset + logical_offset);
        if (dir.inode != 0)
        {
            char filename[EXT2_NAME_LEN + 1];
            memcpy(filename, dir.name, dir.name_len);
            filename[dir.name_len] = 0;
            printf("DIRENT,%d,%d,%u,%u,%u,'%s'\n",
                inode_num,
                logical_offset,
                dir.inode,
                dir.rec_len,
                dir.name_len,
                filename
            );
        }
        logical_offset += dir.rec_len;
    }
}

// indirect block references
void indirect(int level, int inode_num, int block_num, char filetype)
{
    if (level == 1)
    {
        __u32* block_ptrs = (__u32*) malloc(block_size);
        int n = block_size/sizeof(__u32);
        int ind_offset = offset(block_num);
        pread(fd, block_ptrs, block_size, ind_offset);

        int i;
        for (i = 0; i < n; ++i)
        {
            if (block_ptrs[i] != 0) 
            {
                if (filetype == 'd')
                    directory(inode_num, block_ptrs[i]);
                printf("INDIRECT,%d,%d,%d,%d,%d\n",
                    inode_num,
                    level,
                    EXT2_IND_BLOCK + i,
                    block_num,
                    block_ptrs[i]
                );
            }
        }
        free(block_ptrs);
    }
    else if (level == 2)
    {
        __u32* dind_block_ptrs = (__u32*) malloc(block_size);
        int n = block_size/sizeof(__u32);
        int dind_offset = offset(block_num);
        pread(fd, dind_block_ptrs, block_size, dind_offset);
        
        int i;
        for (i = 0; i < n; ++i)
        {
            if (dind_block_ptrs[i] != 0) 
            {
                printf("INDIRECT,%d,%d,%d,%d,%d\n",
                    inode_num,
                    level,
                    256 + EXT2_IND_BLOCK + i,
                    block_num,
                    dind_block_ptrs[i]
                );

                __u32* block_ptrs = (__u32*) malloc(block_size);
                int ind_offset = offset(dind_block_ptrs[i]);
                pread(fd, block_ptrs, block_size, ind_offset);

                int j;
                for (j = 0; j < n; ++j) 
                {
                    if (block_ptrs[j] != 0)
                    {
                        if (filetype == 'd')
                            directory(inode_num, block_ptrs[j]);
                        printf("INDIRECT,%d,%d,%d,%d,%d\n",
                            inode_num,
                            level - 1,
                            256 + EXT2_IND_BLOCK + j,
                            dind_block_ptrs[i],
                            block_ptrs[j]
                        );
                    }
                }
                free(block_ptrs);
            }
        }
        free(dind_block_ptrs);
    }
    else if (level == 3)
    {
        __u32* tind_block_ptrs = (__u32*) malloc(block_size);
        int n = block_size/sizeof(__u32);
        int tind_offset = offset(block_num);
        pread(fd, tind_block_ptrs, block_size, tind_offset);

        int i;
        for (i = 0; i < n; ++i)
        {
            if (tind_block_ptrs[i] != 0) 
            {
                printf("INDIRECT,%d,%d,%d,%d,%d\n",
                    inode_num,
                    level,
                    65536 + 256 + EXT2_IND_BLOCK + i,
                    block_num,
                    tind_block_ptrs[i]
                );

                __u32* dind_block_ptrs = (__u32*) malloc(block_size);
                int dind_offset = offset(tind_block_ptrs[i]);
                pread(fd, dind_block_ptrs, block_size, dind_offset);

                int j;
                for (j = 0; j < n; ++j) 
                {
                    if (dind_block_ptrs[j] != 0)
                    {
                        printf("INDIRECT,%d,%d,%d,%d,%d\n",
                            inode_num,
                            level - 1,
                            65536 + 256 + EXT2_IND_BLOCK + j,
                            tind_block_ptrs[i],
                            dind_block_ptrs[j]
                        );

                        __u32 *block_ptrs = (__u32*) malloc(block_size);
                        int ind_offset = offset(dind_block_ptrs[j]);
                        pread(fd, block_ptrs, block_size, ind_offset);

                        int k;
                        for (k = 0; k < n; ++k) 
                        {
                            if (block_ptrs[k] != 0)
                            {
                                if (filetype == 'd')
                                    directory(inode_num, block_ptrs[k]);
                                printf("INDIRECT,%d,%d,%d,%d,%d\n",
                                    inode_num,
                                    level - 2,
                                    65536 + 256 + EXT2_IND_BLOCK + k,
                                    dind_block_ptrs[j],
                                    block_ptrs[k]
                                );
                            }
                        }
                        free(block_ptrs);
                    }
                }
                free(dind_block_ptrs);
            }
        }
        free(tind_block_ptrs);
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Error, usage is: ./lab3a FILENAME\n");
        exit(1);
    }
    
    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Error, can't open image file\n");
        exit(1);
    }

    superblock();
    
    num_groups = 1 + (super.s_blocks_count - 1) / super.s_blocks_per_group;    
    
    int i;
    for (i = 0; i < num_groups; ++i)
        group(i);

    exit(0);
}
