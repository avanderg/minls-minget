
#define BUF_SIZE 2048
#define MAGIC_PTABLE510 0x55
#define MAGIC_PTABLE511 0xAA
#define PT_ADDR 0x1BE
#define MINIX_PRTMAGIC 0x81
#define MINIX_SBMAGIC 0x4D5A
#define DIRECT_ZONES 7
#define SECTOR_SIZE 512
#define SB_OFFSET 1024
#define PERM_LEN 11

#define FTMASK 0xF000
#define REG_FILE 0x8000
#define DIRECT 0x4000
#define OWN_READ 0x100
#define OWN_WRITE 0x80
#define OWN_EX 0x40
#define GRP_READ 0x20
#define GRP_WRITE 0x10
#define GRP_EX 0x8
#define OTH_READ 0x4
#define OTH_WRITE 0x2
#define OTH_EX 0x1

#define NUM_ARGS 2
#define EMPTY -1
#define NOT_SET 0
#define INO_OFFSET 2
#define SLASH_OFFSET 2
#define DECIMAL 10


#define NO_PERMS "----------"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

struct superblock 
{ 
    /* Minix Version 3 Superblock
     * this structure found in fs/super.h
     * * in minix 3.1.1
     * */
    /* on disk. These fields and orientation are non–negotiable */
    uint32_t ninodes; /* number of inodes in this filesystem */
    uint16_t pad1; /* make things line up properly */
    int16_t i_blocks; /* # of blocks used by inode bit map */
    int16_t z_blocks; /* # of blocks used by zone bit map */
    uint16_t firstdata; /* number of first data zone */
    int16_t log_zone_size; /* log2 of blocks per zone */
    int16_t pad2; /* make things line up again */
    uint32_t max_file; /* maximum file size */
    uint32_t zones; /* number of zones on disk */
    int16_t magic; /* magic number */
    int16_t pad3; /* make things line up again */
    uint16_t blocksize; /* block size in bytes */
    uint8_t subversion; /* filesystem sub–version */
};




struct partition_table 
{
    uint8_t bootind; /*Boot magic number (0x80 if bootable) */
    uint8_t start_head; 
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type; /* Type of partition (0x81 is minix) */
    uint8_t end_head; /*End of partition in CHS */
    uint8_t end_sec;
    uint8_t end_cyl;
    uint32_t lFirst; /*First sector (LBA addressing) */
    uint32_t size; /*size of partition (in sectors) */
};

struct dirent 
{
    uint32_t inode;
    unsigned char name[60];
};

struct inode 
{
    uint16_t mode; /* mode */
    uint16_t links; /* number or links */
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
};

struct node
{
    struct dirent *entry;
    struct node *next;
};
typedef struct node node;


/* Partition Table stuff */
void check_magic_pt(char *buf); 
void pt_setup(int fd, char *buf, int part, struct partition_table *pt); 
uint32_t check_partition_table(int fd, struct partition_table *pt, char *buf, 
        int part, int subpart); 

/* Inode stuff */
void read_inode_table(struct inode *cur, int fd, struct superblock *sb,
        uint32_t lfirst);
void check_perms(struct inode *cur, char *out); 
void reset_perms(char *perms);
void get_ino(uint32_t ino_num, struct superblock *sb, struct inode *ino,
        int fd, uint32_t lfirst);

/* Reading fron filesystem */
node *read_entry(uint32_t zone, struct superblock *sb, int fd, node *head,
        uint32_t size, uint32_t count, uint32_t lfirst);
node *read_path_minget(struct inode* ino, struct superblock *sb, int fd, 
        char *path, char *safe_path, int *file_flag, uint32_t lfirst,
        int fdout);
node *read_path_minls(struct inode* ino, struct superblock *sb, int fd, 
        char *path, char *safe_path, int *file_flag, uint32_t lfirst);
void read_file(struct inode *file, char *safe_path, int fd,
        struct superblock *sb, uint32_t ino_num, uint32_t lfirst);

/* Printing from filesystem */
void print_zone(uint32_t zonesize, struct superblock *sb, int i, uint32_t 
        lfirst, uint32_t zone, int fd, int *size_ctr, uint32_t size,
        int fdout);
void print_file(struct inode *file, char *safe_path, int fd,
        struct superblock *sb, uint32_t ino_num, uint32_t lfirst, int fdout);
void print_verbose(struct superblock *sb, struct inode *cur);

/* List Utilites */
void print_list(node *head);
void free_list(node *head); 
node *add_list(node *head, struct dirent *ptr); 

/* Safe_malloc */
void *safe_malloc(size_t size);
