#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define GROUP_DESCRIPTOR_LENGTH 32
#define INODE_LENGTH 128

struct _superblock{
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_block_size;
    uint32_t s_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_first_data_block;
    uint16_t s_magic;
};

struct _group_descriptor{
    uint32_t bg_num_blocks;
    uint32_t bg_num_inodes;
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
};

struct _inode{
    uint16_t i_mode;
    uint16_t i_links_count;
    uint32_t i_uid;
    uint32_t i_gid;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_atime;
    uint32_t i_block[15];
    uint32_t i_blocks;
    uint64_t i_size;
};

struct _directory_entry{
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[256];
};

typedef struct _superblock superblock;
typedef struct _group_descriptor group_descriptor;
typedef struct _inode inode;
typedef struct _directory_entry directory_entry;

void handle_error(char * msg, int flag);

void parse_superblock(char *a, superblock *sb);
void parse_group_descriptor(char *a, group_descriptor *gp);
void parse_inode(char *a, inode * in);
char file_type(uint16_t mode);

uint32_t read_inode_block(int i, inode* i_node, int fd, superblock sb);
// void read_file_in_block(char * block, FILE * csv);
void parse_directory_entry(char *block, directory_entry *de);

void write_indirect_block(uint32_t i_block, FILE *csv, int fd, superblock sb);
void write_second_block(uint32_t i_block, FILE *csv, int fd, superblock sb);
void write_triple_block(uint32_t i_block, FILE *csv, int fd, superblock sb);

void write_inode_block(inode *i_node, int fd, FILE *csv, superblock sb);

int main (int argc, char ** argv) {
    char *disk_file_name = NULL;

    char super_block[1024];
    int c = 0, fd = 0;
    superblock sb;
    //uint16_t * ptr2 = NULL;
    //uint32_t * ptr4 = NULL;

    long current_offset = 0;

    if (argc > 2) {
        fprintf(stderr, "Parameter error\n");
        exit(1);
    }

    disk_file_name = strdup(argv[1]);
    fd = open(disk_file_name, O_RDONLY);

    if (fd <= 0) handle_error("fopen() fail", 1);


    c = pread(fd, super_block, 1024, 1024);
    if (c != 1024) {
        fprintf(stderr, "%s\n", "fread() fail, superblock read error");
        exit(1);
    }

    // super csv
    parse_superblock(super_block, &sb);
    FILE * super_csv = fopen("super.csv", "w");
    fprintf(super_csv, "%x,%d,%d,%d,%d,%d,%d,%d,%d\n", sb.s_magic,sb.s_inodes_count,\
    sb.s_blocks_count,sb.s_block_size,sb.s_frag_size,sb.s_blocks_per_group,\
    sb.s_inodes_per_group,sb.s_frags_per_group,sb.s_first_data_block);
    fclose(super_csv);

    // group descriptor
    current_offset = (sb.s_first_data_block+1)*sb.s_block_size;
    char *group_char = malloc(sb.s_block_size*sizeof(char));
    if (group_char == NULL) handle_error("malloc() fail", 1);
    c = pread(fd, group_char, sb.s_block_size, current_offset);
    if (c != sb.s_block_size) {
        fprintf(stderr, "%s\n", "group descriptor read error");
        exit(1);
    }
    int num_of_groups = sb.s_blocks_count/sb.s_blocks_per_group;
    long total_blocks = sb.s_blocks_count;
    long total_inodes = sb.s_inodes_count;

    if (sb.s_block_size % sb.s_blocks_per_group != 0) num_of_groups++;
    group_descriptor *gp = malloc(num_of_groups*sizeof(group_descriptor));
    if (gp == NULL) handle_error("malloc() fail", 1);
    memset(gp, 0, num_of_groups*sizeof(group_descriptor));

    for (int i = 0; i < num_of_groups; i++) {
        parse_group_descriptor(group_char+i*GROUP_DESCRIPTOR_LENGTH, gp+i);
        if (total_blocks >= sb.s_blocks_per_group) {
            (gp+i)->bg_num_blocks = sb.s_blocks_per_group;
            total_blocks -= sb.s_blocks_per_group;
        }else {
            (gp+i)->bg_num_blocks = total_blocks;
        }

        if (total_inodes >= sb.s_inodes_per_group) {
            (gp+i)->bg_num_inodes = sb.s_inodes_per_group;
            total_inodes -= sb.s_inodes_per_group;
        }else {
            (gp+i)->bg_num_inodes = total_inodes;
        }
    }
    FILE *group_csv = fopen("group.csv", "w");
    group_descriptor * cur = NULL;
    for (int i = 0; i < num_of_groups; i++) {
        cur = gp+i;
        fprintf(group_csv, "%d,%d,%d,%d,%x,%x,%x\n", cur->bg_num_blocks,\
        cur->bg_free_blocks_count,cur->bg_free_inodes_count,\
        cur->bg_used_dirs_count,cur->bg_inode_bitmap, cur->bg_block_bitmap,\
        cur->bg_inode_table);
    }
    fclose(group_csv);


    // bitmap.csv
    long inode_index = 0, block_index = 0;
    long current_inode_index = 0, current_block_index = 0;
    char * inode_bitmap, *block_bitmap;
    inode_bitmap = malloc(sb.s_block_size*sizeof(char));
    if (inode_bitmap == NULL) handle_error("malloc() fail", 1);
    block_bitmap = malloc(sb.s_block_size*sizeof(char));
    if (block_bitmap == NULL) handle_error("malloc() fail", 1);

    FILE *bitmap_csv = fopen("bitmap.csv", "w");
    char tmp;
    for (int i = 0; i < num_of_groups; i++) {
        memset(inode_bitmap, 0, sb.s_block_size*sizeof(char));
        memset(block_bitmap, 0, sb.s_block_size*sizeof(char));
        c = pread(fd, inode_bitmap, sb.s_block_size, sb.s_block_size*(gp+i)->bg_inode_bitmap);
        c = pread(fd, block_bitmap, sb.s_block_size, sb.s_block_size*(gp+i)->bg_block_bitmap);
        // fprintf(stderr, "%x,%x\n", (gp+i)->bg_block_bitmap,(gp+i)->bg_inode_bitmap);
        for (int j = 0; j < (gp+i)->bg_num_blocks/8+1; j++) {
            tmp = block_bitmap[j];
            for (int k = 0; k < 8; k++) {
                current_block_index++;
                if (current_block_index > (gp+i)->bg_num_blocks) break;
                block_index++;
                if ((tmp & (1 << k)) == 0) {
                    fprintf(bitmap_csv, "%x,%ld\n", (gp+i)->bg_block_bitmap, block_index);
                }
            }
        }

        for (int j = 0; j < (gp+i)->bg_num_inodes/8+1; j++) {
            tmp = inode_bitmap[j];
            for (int k = 0; k < 8; k++) {
                current_inode_index++;
                if (current_inode_index > (gp+i)->bg_num_inodes) break;
                inode_index++;
                if ((tmp & (1 << k)) == 0) {
                    fprintf(bitmap_csv, "%x,%ld\n", (gp+i)->bg_inode_bitmap, inode_index);
                }
            }
        }

        current_block_index = 0;
        current_inode_index = 0;
    }
    fclose(bitmap_csv);


    // inode.csv
    FILE * inode_csv = fopen("inode.csv", "w");
    char * inode_table = malloc(INODE_LENGTH*sb.s_inodes_per_group);
    if (inode_table == NULL) handle_error("malloc() fail", 1);

    inode_index = 0;
    current_inode_index = 0;
    inode i_node;

    for (int i = 0; i < num_of_groups; i++) {
        current_offset = sb.s_block_size*(gp+i)->bg_inode_table;
        c = pread(fd, inode_table, INODE_LENGTH*(gp+i)->bg_num_inodes,\
         current_offset);
         c = pread(fd, inode_bitmap, sb.s_block_size, sb.s_block_size*(gp+i)->bg_inode_bitmap);
        for (int j = 0; j < (gp+i)->bg_num_inodes/8+1; j++) {
            tmp = inode_bitmap[j];
            // if (i == 0) fprintf(stderr, "i=%d , %02x\n", i, tmp);
            for (int k = 0; k < 8; k++) {
                current_inode_index++;
                if (current_inode_index > (gp+i)->bg_num_inodes) break;
                inode_index++;

                if ((tmp & (1 << k)) != 0) {
                    parse_inode(inode_table+(current_inode_index-1)*INODE_LENGTH,\
                     &i_node);
                     i_node.i_blocks = i_node.i_blocks*512/sb.s_block_size;
                     fprintf(inode_csv, "%ld,%c,%o,%d,%d,%d,%x,%x,%x,%ld,%d,",\
                        inode_index, file_type(i_node.i_mode),i_node.i_mode,\
                        i_node.i_uid, i_node.i_gid, i_node.i_links_count, i_node.i_ctime,\
                    i_node.i_mtime, i_node.i_atime, i_node.i_size, i_node.i_blocks);
                    for (int t = 0; t < 15; t++) {
                        fprintf(inode_csv, "%x%c", i_node.i_block[t],(t == 14? '\n':','));
                    }
                }
            }
        }
        current_inode_index = 0;
    }
    fclose(inode_csv);


    // directory.csv
    FILE * directory_csv = fopen("directory.csv", "w");
    current_inode_index = 0;
    inode_index = 0;
    long entity_index = 0;
    int size = 0;
    char * block = malloc(sb.s_block_size);
    for (int i = 0; i < num_of_groups; i++) {
        current_offset = sb.s_block_size*(gp+i)->bg_inode_table;
        c = pread(fd, inode_table, INODE_LENGTH*(gp+i)->bg_num_inodes,\
         current_offset);
         c = pread(fd, inode_bitmap, sb.s_block_size, sb.s_block_size*(gp+i)->bg_inode_bitmap);
        for (int j = 0; j < (gp+i)->bg_num_inodes/8+1; j++) {
            tmp = inode_bitmap[j];
            // if (i == 0) fprintf(stderr, "i=%d , %02x\n", i, tmp);
            for (int k = 0; k < 8; k++) {
                current_inode_index++;
                if (current_inode_index > (gp+i)->bg_num_inodes) break;
                inode_index++;

                if ((tmp & (1 << k)) != 0) {
                    parse_inode(inode_table+(current_inode_index-1)*INODE_LENGTH,\
                     &i_node);
                     i_node.i_blocks = i_node.i_blocks*512/sb.s_block_size;
                    if ((i_node.i_mode&0x4000) != 0) {
                        // directory inode
                        entity_index = 0;
                        int i_block = 0;
                        uint32_t res = 0;

                        directory_entry entry;
                        memset(&entry, 0, sizeof(directory_entry));

                        while (1) {
                            // read all the block
                            size = 0;
                            res = read_inode_block(i_block, &i_node, fd, sb);
                            i_block++;
                            if (res == 0) break;
                            pread(fd, block, sb.s_block_size, res*sb.s_block_size);
                            while (size < sb.s_block_size) {
                                parse_directory_entry(block+size, &entry);
                                size += entry.rec_len;
                                if (entry.inode != 0) {
                                    fprintf(directory_csv, "%ld,%ld,%d,%d,%d,\"%s\"\n", inode_index,\
                                    entity_index,entry.rec_len,entry.name_len,entry.inode,entry.name);
                                }
                                entity_index++;
                            }
                        }
                    }
                }
            }
        }
        current_inode_index = 0;
    }
    fclose(directory_csv);



    // indirect.csv
    FILE * indirect_csv = fopen("indirect.csv", "w");
    for (int i = 0; i < num_of_groups; i++) {
        current_offset = sb.s_block_size*(gp+i)->bg_inode_table;
        c = pread(fd, inode_table, INODE_LENGTH*(gp+i)->bg_num_inodes,\
         current_offset);
         c = pread(fd, inode_bitmap, sb.s_block_size, sb.s_block_size*(gp+i)->bg_inode_bitmap);
        for (int j = 0; j < (gp+i)->bg_num_inodes/8+1; j++) {
            tmp = inode_bitmap[j];
            // if (i == 0) fprintf(stderr, "i=%d , %02x\n", i, tmp);
            for (int k = 0; k < 8; k++) {
                current_inode_index++;
                if (current_inode_index > (gp+i)->bg_num_inodes) break;
                inode_index++;

                if ((tmp & (1 << k)) != 0) {
                    parse_inode(inode_table+(current_inode_index-1)*INODE_LENGTH,\
                     &i_node);
                     i_node.i_blocks = i_node.i_blocks*512/sb.s_block_size;
                     // i_node
                     write_inode_block(&i_node, fd, indirect_csv, sb);
                }
            }
        }
        current_inode_index = 0;
    }
    fclose(indirect_csv);

    close(fd);
    return 0;

}

void parse_superblock(char *a, superblock *sb) {
    memset(sb, 0, sizeof(superblock));
    uint32_t * ptr4 = NULL;
    uint16_t *ptr2 = NULL;
    int * ptint = NULL;

    ptr4 = (uint32_t*)a;
    ptr2 = (uint16_t*)a;
    sb->s_magic = *(ptr2+28);
    sb->s_inodes_count = *ptr4;
    sb->s_blocks_count = *(ptr4+1);
    sb->s_first_data_block = *(ptr4+5);
    sb->s_block_size = 1024 << *(ptr4+6);
    sb->s_blocks_per_group = *(ptr4+8);
    sb->s_frags_per_group = *(ptr4+9);
    sb->s_inodes_per_group = *(ptr4+10);
    ptint = (int*)a;
    ptint += 7;
    if (*ptint > 0) {
        sb->s_frag_size = 1024 << *ptint;
    }else {
        sb->s_frag_size = 1024 >> -(*ptint);
    }
}

void parse_group_descriptor(char *a, group_descriptor *gp) {
    memset(gp, 0, sizeof(group_descriptor));
    uint32_t * ptr4 = NULL;
    uint16_t *ptr2 = NULL;

    ptr4 = (uint32_t*)a;
    gp->bg_block_bitmap = *ptr4;
    gp->bg_inode_bitmap = *(ptr4+1);
    gp->bg_inode_table = *(ptr4+2);

    ptr2 = (uint16_t*)a;
    gp->bg_free_blocks_count = *(ptr2+6);
    gp->bg_free_inodes_count = *(ptr2+7);
    gp->bg_used_dirs_count = *(ptr2+8);
}

void parse_inode(char *a, inode * in) {
    memset(in, 0, sizeof(inode));
    uint16_t *ptr2;
    uint32_t *ptr4;
    uint16_t a16;
    uint32_t a32;
    uint64_t a64;
    ptr2 = (uint16_t*)a;
    ptr4 = (uint32_t*)a;

    in->i_mode = *(ptr2);
    in->i_atime = *(ptr4+2);
    in->i_ctime = *(ptr4+3);
    in->i_mtime = *(ptr4+4);
    in->i_links_count = *(ptr2+13);
    // in->i_block = *(ptr4+10);
    in->i_blocks = *(ptr4+7);
    memcpy(in->i_block, ptr4+10, 15*4);

    in->i_uid |= *(ptr2+1);
    a16 = *(ptr2+60);
    a32 = a16;
    in->i_uid |= (a32 << 16);

    a16 = *(ptr2+61);
    a32 = a16;
    in->i_gid = *(ptr2+12);
    in->i_gid |= (a32 << 16);

    in->i_size |= *(ptr4+1);
    a32 = *(ptr4+27);
    a64 = a32;
    in->i_size |= (a64 << 32);

}

void parse_directory_entry(char *block, directory_entry *de) {
    memset(de, 0, sizeof(directory_entry));
    uint32_t *ptr4;
    uint16_t *ptr2;
    uint8_t *ptr1;
    ptr4 = (uint32_t*)block;
    ptr2 = (uint16_t*)block;
    ptr1 = (uint8_t*)block;
    de->inode = *ptr4;
    de->rec_len = *(ptr2+2);
    de->name_len = *(ptr1+6);
    de->file_type = *(ptr1+7);

    memcpy(de->name, ptr1+8, de->name_len);
    de->name[de->name_len] = 0;
}

//
uint32_t read_inode_block(int i, inode* i_node, int fd, superblock sb) {
    if (i < 12) {
        return i_node->i_block[i];
    }
    int p = sb.s_block_size/4;
    uint32_t *ptr4 = NULL;
    uint32_t res = 0;

    char *block1 = NULL, *block2 = NULL, *block3 = NULL;
    if (i < 12+p) {
        if (i_node->i_block[12] == 0) return 0;
        block1 = malloc(sb.s_block_size);
        pread(fd, block1, sb.s_block_size, i_node->i_block[12]*sb.s_block_size);
        ptr4 = (uint32_t*)block1;
        res = ptr4[i-12];
        free(block1);
        return res;
    }
    if (i < 12+p+p*p) {
        if (i_node->i_block[13] == 0) return 0;
        int m = 0, n = 0;
        i = i - p - 12;
        m = i / p;
        n = i % p;
        block1 = malloc(sb.s_block_size);
        pread(fd, block1, sb.s_block_size, i_node->i_block[13]*sb.s_block_size);
        ptr4 = (uint32_t*)block1;
        if (*(ptr4+m) == 0) {
            free(block1);
            return 0;
        }
        block2 = malloc(sb.s_block_size);
        pread(fd, block2, sb.s_block_size, ptr4[m]*sb.s_block_size);
        ptr4 = (uint32_t*)block2;
        res = ptr4[n];
        free(block1);
        free(block2);
        return res;
    }
    if (i < 12+p+p*p+p*p*p) {
        if (i_node->i_block[14] == 0) return 0;
        int m = 0, n = 0, k = 0;
        i = i - 12 - p - p*p;
        m = i / (p*p);
        i = i % (p*p);
        n = i / p;
        k = i % p;
        block1 = malloc(sb.s_block_size);
        pread(fd, block1, sb.s_block_size, i_node->i_block[14]*sb.s_block_size);
        if (block1[m] == 0) {
            free(block1);
            return 0;
        }
        block2 = malloc(sb.s_block_size);
        pread(fd, block2, sb.s_block_size, block1[m]*sb.s_block_size);
        if (block2[n] == 0) {
            free(block1);
            free(block2);
            return 0;
        }
        block3 = malloc(sb.s_block_size);
        pread(fd, block3, sb.s_block_size, block2[n]*sb.s_block_size);
        res = block3[k];
        free(block1);
        free(block2);
        free(block3);
        return res;
    }
    return 0;
}

void write_indirect_block(uint32_t i_block, FILE *csv, int fd, superblock sb) {
    char *block = malloc(sb.s_block_size);
    int n = sb.s_block_size/4;
    memset(block, 0, sizeof(sb.s_block_size));
    pread(fd, block, sb.s_block_size, i_block*sb.s_block_size);
    uint16_t entry_index = 0;
    uint32_t *ptr4 = NULL;

    ptr4 = (uint32_t*)block;
    for (int i = 0; i < n; i++) {
        if (*(ptr4+i) == 0) break;
        fprintf(csv, "%x,%d,%x\n", i_block, entry_index, *(ptr4+i));
        entry_index++;
    }
    free(block);
    return;
}

void write_second_block(uint32_t i_block, FILE *csv, int fd, superblock sb) {
    char *block = malloc(sb.s_block_size);
    int n = sb.s_block_size/4;
    memset(block, 0, sizeof(sb.s_block_size));
    pread(fd, block, sb.s_block_size, i_block*sb.s_block_size);
    uint16_t entry_index = 0;
    uint32_t *ptr4 = NULL;

    ptr4 = (uint32_t*)block;
    for (int i = 0; i < n; i++) {
        if (*(ptr4+i) == 0) break;
        fprintf(csv, "%x,%d,%x\n", i_block, entry_index, *(ptr4+i));
        entry_index++;
    }

    for (int i = 0; i < n; i++) {
        if (*(ptr4+i) == 0) break;
        write_indirect_block(*(ptr4+i), csv, fd, sb);
    }
    free(block);

    return;
}

void write_triple_block(uint32_t i_block, FILE *csv, int fd, superblock sb) {
    char *block = malloc(sb.s_block_size);
    int n = sb.s_block_size/4;
    memset(block, 0, sizeof(sb.s_block_size));
    pread(fd, block, sb.s_block_size, i_block*sb.s_block_size);
    uint16_t entry_index = 0;
    uint32_t *ptr4 = NULL;

    ptr4 = (uint32_t*)block;
    for (int i = 0; i < n; i++) {
        if (*(ptr4+i) == 0) break;
        fprintf(csv, "%x,%d,%x\n", i_block, entry_index, *(ptr4+i));
        entry_index++;
    }

    for (int i = 0; i < n; i++) {
        if (*(ptr4+i) == 0) break;
        write_second_block(*(ptr4+i), csv, fd, sb);
    }
    free(block);

    return;
}



void write_inode_block(inode *i_node, int fd , FILE *csv, superblock sb) {
    // if (i_node->i_block[12] != 0) {
    //     write_indirect_block(i_node->i_block[12], csv, fd, sb);
    // }
    // if (i_node->i_block[13] != 0) {
    //     write_second_block(i_node->i_block[13], csv, fd, sb);
    // }
    // if (i_node->i_block[14] != 0) {
    //     write_triple_block(i_node->i_block[14], csv, fd, sb);
    // }
    write_indirect_block(i_node->i_block[12], csv, fd, sb);
    write_second_block(i_node->i_block[13], csv, fd, sb);
    write_triple_block(i_node->i_block[14], csv, fd, sb);

}



char file_type(uint16_t mode) {
    if ((mode & 0x8000) != 0) return 'f';
    if ((mode & 0x4000) != 0) return 'd';
    if ((mode & 0xA000) != 0) return 's';
    return '?';
}

void handle_error(char *msg, int flag) {
    perror(msg);
    exit(flag);
}
