/* Name: Denis Pyryev (dpyryev)
 *	     Aaron VanderGraaff (avanderg)
 *
 * Date: June 5, 2019
 * Assignment: Program 5
 * Instructor: Professor Phillip Nico
 * Course: CPE 453-03

 * Description: All of our functions for minls and minget since 
 * there is a lot of overlap, we put them in the same file 
 */

#include "utilities.h"

/********************************************/
/*  Partition Table Stuff */
/*******************************************/

/* Check magic number of a partition table */
void check_magic_pt(char *buf) 
{
    uint8_t b511;
    uint8_t b510;

    b510 = buf[510];
    b511 = buf[511];

    /* Compare byte 510 and 511 to 0x55 and 0xAA */
    if (b510 != MAGIC_PTABLE510 || 
        b511 != MAGIC_PTABLE511)
    {
            printf("No partition table found\n");
            exit(EXIT_FAILURE);
    }
}

/* Load the partition table into a partition table struct */
void pt_setup(int fd, char *buf, int part, struct partition_table *pt) 
{
    uint32_t first;
    struct partition_table *tmp_pt;


    /* Go to 0x1BE and get the partition table */
    tmp_pt = (struct partition_table *)&buf[PT_ADDR]; 
    /* Go to the specified partition*/
    tmp_pt = tmp_pt + part;

    /* Check magic for minix */
    if (tmp_pt->type != MINIX_PRTMAGIC) 
    {
        printf("This doesn't look like a minix filesystem\n");
        exit(EXIT_FAILURE);
    }


    /* Seek to beginning of partition */
    first = tmp_pt->lFirst*SECTOR_SIZE;
    if (lseek(fd, first, SEEK_SET) == -1)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    pt = memcpy(pt, tmp_pt, sizeof(struct partition_table));
           
    /* Read first part of partition into buf */
    if (read(fd, buf, BUF_SIZE) < 0) 
    {
        perror("read");
        exit(EXIT_FAILURE);
    }


}

/* Check for a valid partition table and set up pt struct with data */
uint32_t check_partition_table(int fd, struct partition_table *pt, char *buf, 
        int part, int subpart) 
{
    /* If the user didn't ask to look for one, don't */
    if (part == -1)
    {
        return 0;
    }

    /* Check magic numbers */
    check_magic_pt(buf);
    /* Setup the parittion table, load data into pt struct */
    pt_setup(fd, buf, part, pt);

    /* if the user asked for a sub partition, do the same again */
    if (subpart != -1) 
    {

        check_magic_pt(buf); 
        pt_setup(fd, buf, subpart, pt);

    }
    return pt->lFirst;
}

/********************************************/
/* Inode stuff */
/*******************************************/

/* Reads the first inode in the inode table */
void read_inode_table(struct inode *cur, int fd, struct superblock *sb, 
        uint32_t lfirst) 
{
    
    int inode_start, num;
    off_t check;
    /* Calculate the offset of the inode table and seek there */

    inode_start = SECTOR_SIZE*lfirst + sb->blocksize*
        (2 + sb->i_blocks + sb->z_blocks);
    if ((check = lseek(fd, inode_start, SEEK_SET)) < 0) 
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    /* If we didn't seek to the right spot, blow up */
    if (check != inode_start)
    {
        printf("check: %lu and inode_start: %d don't agree\n", check, 
                inode_start);
        exit(EXIT_FAILURE);
    }
    
    /* Read the inode, root */
    if ((num = read(fd, cur, sizeof(struct inode))) < 0) 
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
}

/* Sets a permission string given the permisions of a file */
void check_perms(struct inode *cur, char *out) 
{
    uint16_t mode;
    
    mode = cur->mode;
    

    if (mode & DIRECT)
    {
        out[0] = 'd';
    }

    if (mode & OWN_READ) 
    {
        out[1] = 'r';
    }
    
    if (mode & OWN_WRITE) 
    {
        out[2] = 'w';
    }

    if (mode & OWN_EX) 
    {
        out[3] = 'x';
    }
   
    if (mode & GRP_READ) 
    {
        out[4] = 'r';
    }
    
    if (mode & GRP_WRITE) 
    {
        out[5] = 'w';
    }

    if (mode & GRP_EX) 
    {
        out[6] = 'x';
    }

    if (mode & OTH_READ) 
    {
        out[7] = 'r';
    }

    if (mode & OTH_WRITE) 
    {
        out[8] = 'w';
    }
    
    if (mode & OTH_EX) 
    {
        out[9] = 'x';
    }
}

/* Reset permission string */
void reset_perms(char *perms)
{
    int i;
    for (i=0; i < PERM_LEN-1; i++)
    {
        perms[i] = '-';
    }
}

/* Get an ino from the ino table */
void get_ino(uint32_t ino_num, struct superblock *sb, struct inode *ino, 
        int fd, uint32_t lfirst)
{
    int inode_start; 


    inode_start = lfirst*SECTOR_SIZE +
                  sb->blocksize*(2 + sb->i_blocks + sb->z_blocks);
    /* Seek to the right block and read it */
    if ((lseek(fd, inode_start + sizeof(struct inode) * 
                    (ino_num - 1), SEEK_SET)) == -1) 
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    if (read(fd, ino, sizeof(struct inode)) < 0) 
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
}

/********************************************/
/* Reading the Filesystem */
/*******************************************/

/* Read a directory entry and append it to our entry list */
node *read_entry(uint32_t zone, struct superblock *sb, int fd, node *head,
        uint32_t size, uint32_t count, uint32_t lfirst)
{

    uint32_t zonesize;
    int block;
    char buf[sb->blocksize];
    int num;
    struct dirent *ptr;
    uint32_t counter;
    int i;

    i = 0;
    counter = 0;

    /* Seek to the right zone */
    zonesize = sb->blocksize << sb->log_zone_size;
    block = zone*zonesize + lfirst*SECTOR_SIZE;
    
    if (lseek(fd, block, SEEK_SET) < 0) 
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
   
    /* Read the zones one block at a time */
    for (i = 0; i < (zonesize/sb->blocksize); i++)
    {

        if ((num = read(fd, buf, sb->blocksize)) < 0)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        ptr = (struct dirent *)buf;

        /* Add every entry to the linked list */
        while (counter < sb->blocksize) 
        {
            if (ptr->inode)
            {
                head = add_list(head, ptr);  
            }

            counter += sizeof(struct dirent);
            ptr++;
        }
        counter = 0;
    }
    /* Return the list */
    return head;
}

/* Both read_path_minls and read_path_minget are very similar
   The main difference is one calls read_file and the other calls 
   print file, along with other error checking differences (ie 
   you can minls a directory but not a file */
node *read_path_minget(struct inode* ino, struct superblock *sb, int fd, 
        char *path, char *safe_path, int *file_flag, uint32_t lfirst, 
        int fdout)

{
    char *file;
    node *head = NULL;
    node *tmp;
    struct inode next_ino;
    int i;
    uint32_t zone;
    char *s;
    char *new_path;
    char buf[sb->blocksize];
    char buf2[sb->blocksize];
    uint32_t zone2;
    int j;
    uint32_t zonesize;
    uint32_t *zptr;
    uint32_t *zptr2;
    
    /* Read all the entries in the direct zones */
    zonesize = sb->blocksize << sb->log_zone_size;
    i = 0;
    zone = ino->zone[i];
    while (i < DIRECT_ZONES) 
    {
        if (!zone)
        {
            zone = ino->zone[++i];
            continue;
        }
        head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
        zone = ino->zone[++i];
    }

    /* Read all the entries in the indirect zones, this is similar to
       print_file except for we skip empty zones and call read_entry
       instead of print_zone */
    if (ino->indirect)
    {

        /* Seek to the right block and read it, this is a block of 
           zone numbers */
        if (lseek(fd, ino->indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }

        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* Loop through all these zone numbers and call read_entry on
           them */
        zptr = (uint32_t *) buf;
        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];
            /* Don't read 0 zones as entries */
            if (!zone) 
            {
                continue;
            }
            head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
        }
    }

    /* Double indirect entry read */ 
    if (ino->two_indirect)
    {
        
        /* Seek to and read the right block */
        if (lseek(fd, ino->two_indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }

        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        zptr = (uint32_t *) buf;

        /* Read through all the zone numbers and treat them each
           as an indirect block */
        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];
            if (!zone)
            {
                continue;
            }

            if (lseek(fd, zone*zonesize + lfirst*SECTOR_SIZE,
                        SEEK_SET) < 0) 
            {
                perror("lseek");
                exit(EXIT_FAILURE);
            }

            if (read(fd, buf2, sb->blocksize) < 0) 
            {
                perror("read");
                exit(EXIT_FAILURE);
            }

            /* This is just the indirect block stuff */
            zptr2 = (uint32_t *) buf2;

            for (j=0; j<sb->blocksize/sizeof(uint32_t); j++)
            {
                zone2 = zptr2[j];
                if (!zone2)
                {
                    continue;
                }
                head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
            }

        }

    }


    /* A linked list starting at head holds all the data we need */
    /* Split the given path by /s and look for our file name */
    file = strtok_r(path, "/", &new_path);
    tmp = head;
    /* If there's more to the path */
    if (file)
    {

        /* Loop through list */
        while (tmp)
        {
            s = (char *)tmp->entry->name;
            
            if (!strcmp(s, file))
            {
                break;
            }
            tmp = tmp->next;
        }

        /* Didn't find file */
        if (!tmp)
        {
            printf("%s: File not found.\n", safe_path);
            exit(EXIT_FAILURE);
        }

        /* Get the ino and check what it is */
        get_ino(tmp->entry->inode, sb, &next_ino, fd, lfirst);
        /* If it's a directory, free it and make a new list */
        if ((next_ino.mode & FTMASK) ==  DIRECT)
        {
            if (head) 
            {
                free_list(head);
            }

            /* We ended on a directory, that's a no go for minget */
            if (strlen(new_path) == 0)
            {
                printf("You're reading a directory as a file\n");
                exit(EXIT_FAILURE);
            }

            /* Read the next directory */
            head = read_path_minget(&next_ino, sb, fd, new_path, safe_path, 
                    file_flag, lfirst, fdout);
        }
        else if ((next_ino.mode & FTMASK) == REG_FILE)
        {

            /* We have a file in the middle of the path, that's bad */
            if (strlen(new_path) != 0)
            {
                printf("You're reading a file as a directory\n");
                exit(EXIT_FAILURE);
            }
            /* Otherwise we found our  file, print it */
            print_file(&next_ino, safe_path, fd, sb, tmp->entry->inode, 
              lfirst, fdout); 
            *file_flag = 1;
            return head;
        }
        /* If it's not a regular file or directory, freak out */
        else 
        {
            printf("This is not a regular file\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Return the list */
    return head;
}

/* Almost the same as minget, up to the end */
node *read_path_minls(struct inode* ino, struct superblock *sb, int fd, 
        char *path, char *safe_path, int *file_flag, uint32_t lfirst)

{
    char *file;
    node *head = NULL;
    node *tmp;
    struct inode next_ino;
    int i;
    uint32_t zone;
    char *s;
    char *new_path;
    char buf[sb->blocksize];
    char buf2[sb->blocksize];
    uint32_t zone2;
    int j;
    uint32_t zonesize;
    uint32_t *zptr;
    uint32_t *zptr2;

    /* Same as minget, probably could be lumped into a separate function */
    zonesize = sb->blocksize << sb->log_zone_size;
    i = 0;
    zone = ino->zone[i];
    while (zone) 
    {
        head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
        zone = ino->zone[++i];
    }

    /* Indirect is also the same */
    if (ino->indirect)
    {

        if (lseek(fd, ino->indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }

        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        zptr = (uint32_t *) buf;
        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];
            if (!zone) 
            {
                continue;
            }
            head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
        }
    }

    /* 2 indirect is the same too */
    if (ino->two_indirect)
    {
        
        if (lseek(fd, ino->two_indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }

        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        zptr = (uint32_t *) buf;

        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];
            if (!zone)
            {
                continue;
            }

            if (lseek(fd, zone*zonesize + lfirst*SECTOR_SIZE,
                        SEEK_SET) < 0) 
            {
                perror("lseek");
                exit(EXIT_FAILURE);
            }

            if (read(fd, buf2, sb->blocksize) < 0) 
            {
                perror("read");
                exit(EXIT_FAILURE);
            }

            zptr2 = (uint32_t *) buf2;

            for (j=0; j<sb->blocksize/sizeof(uint32_t); j++)
            {
                zone2 = zptr2[j];
                if (!zone2)
                {
                    continue;
                }
                head = read_entry(zone, sb, fd, head, ino->size, 0, lfirst);
            }

        }

    }

    /* Finally read in the listing of our current directory */

    file = strtok_r(path, "/", &new_path);
    
    /* Loop through this directory (just like minget) */
    tmp = head;
    if (file)
    {

        while (tmp)
        {
            s = (char *)tmp->entry->name;
            
            if (!strcmp(s, file))
            {
                break;
            }
            tmp = tmp->next;
        }

        if (!tmp)
        {
            printf("%s: File not found.\n", safe_path);
            exit(EXIT_FAILURE);
        }

        get_ino(tmp->entry->inode, sb, &next_ino, fd, lfirst);
        if ((next_ino.mode & FTMASK) ==  DIRECT)
        {
            if (head) 
            {
                free_list(head);
            }
            /* No edge case for ending on a directory, that's all good */

            head = read_path_minls(&next_ino, sb, fd, new_path, safe_path, 
                    file_flag, lfirst);
        }
        else if ((next_ino.mode & FTMASK) == REG_FILE)
        {
            if (strlen(new_path) != 0)
            {
                printf("You're reading a file as a directory\n");
                exit(EXIT_FAILURE);
            }
            /* If we end on a file, that's a special case, print 
               the file's metadata instead of the file's data */
            read_file(&next_ino, safe_path, fd, sb, tmp->entry->inode, 
                    lfirst);
            *file_flag = 1;
            return head;
        }

    }

    return head;
}

/* Reads off a file's metadata for minls */
void read_file(struct inode *file, char *safe_path, int fd,
        struct superblock *sb, uint32_t ino_num, uint32_t lfirst) 
{

    char perms[11] = NO_PERMS;
    int inode_start;

    /* Seek to the proper place and read in the inode */
    inode_start = lfirst*SECTOR_SIZE + 
        sb->blocksize*(2 + sb->i_blocks + sb->z_blocks);
    if ((lseek(fd, inode_start + sizeof(struct inode) * 
                    (ino_num - 1), SEEK_SET)) < 0) 
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    if (read(fd, file, sizeof(struct inode)) < 0) 
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    /* Get the permisions and size and print them */
    check_perms(file, perms);
    printf("%s%10d %s\n", perms, file->size, safe_path); 

}

/********************************************/
/* Printing from the filesystem */
/*******************************************/

/* Print the contents of the given zone */
void print_zone(uint32_t zonesize, struct superblock *sb, int i, uint32_t 
        lfirst, uint32_t zone, int fd, int *size_ctr, uint32_t inosize,
        int fdout)
{
    int j;
    char buf[sb->blocksize];
    int num;

    /* Seek to the zone if it exists, otherwise set buf to all 0s */
    if (zone)
    {
        if (lseek(fd, zone*zonesize + lfirst*SECTOR_SIZE, SEEK_SET) < 0)
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }
    }

    else
    {
        num = sb->blocksize;
        memset(buf, 0, sb->blocksize);
    }

    for (j=0; j<zonesize/sb->blocksize; j++)
    {

        /* Read a block from the zone if it exists */
        if (zone)
        {
            if ((num = read(fd, buf, sb->blocksize)) < 0)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }
        }

        if (num)
        {
            /* check the file size */
            *size_ctr += num;
            if (*size_ctr > inosize)
            {
                num = sb->blocksize - (*size_ctr - inosize);
            }
            if (num <= 0)
            {
                return;
            }
            /* Write the block */
            if (write(fdout, buf, num) < 0)
            {
                perror("write");
                exit(EXIT_FAILURE);
            
            }
        }
    }
}

/* Print out the file for minget */
void print_file(struct inode *file, char *safe_path, int fd,
        struct superblock *sb, uint32_t ino_num, uint32_t lfirst, int fdout) 
{

    int inode_start;
    uint32_t zonesize;
    int i;
    int j;
    uint32_t zone;
    char buf[sb->blocksize];
    uint32_t *zptr;
    uint32_t zone2;
    char buf2[sb->blocksize];
    uint32_t *zptr2;
    int size_ctr;

    size_ctr = 0;

    zonesize = sb->blocksize << sb->log_zone_size;
    inode_start = lfirst*SECTOR_SIZE + 
        sb->blocksize*(2 + sb->i_blocks + sb->z_blocks);

    /* Seek to appropriate block and read it */
    if ((lseek(fd, inode_start + sizeof(struct inode) * 
                    (ino_num - 1), SEEK_SET)) < 0) 
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    if (read(fd, file, sizeof(struct inode)) < 0) 
    {
        perror("read");
        exit(EXIT_FAILURE);
    }

    /* Read and print the direct zones */
    i = 0;
    zone = file->zone[i];
    while (i < DIRECT_ZONES) 
    {
        print_zone(zonesize, sb, i, lfirst, zone, fd, &size_ctr, file->size,
                fdout);
        zone = file->zone[++i];
    }

    /* Read and print the indirect zones */
    if (file->indirect)
    {
        /* Seek to the starting block of zone numbers and read it */

        if (lseek(fd, file->indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }


        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* Go to each zone and print it */
        zptr = (uint32_t *) buf;
        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];
            print_zone(zonesize, sb, i, lfirst, zone, fd, &size_ctr, 
                    file->size, fdout);
        }
    }

    /* Double indirect, do the same as indirect but twice */
    if (file->two_indirect)
    {
        
        if (lseek(fd, file->two_indirect*zonesize + lfirst*SECTOR_SIZE,
                    SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }


        zptr = (uint32_t *) buf;

        if (read(fd, buf, sb->blocksize) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* Loop through all these blocks and treat them as indirect */
        for (i=0; i<sb->blocksize/sizeof(uint32_t); i++)
        {
            zone = zptr[i];

            if (lseek(fd, zone*zonesize + lfirst*SECTOR_SIZE,
                        SEEK_SET) < 0) 
            {
                perror("lseek");
                exit(EXIT_FAILURE);
            }


            /* This is the same as indirect */
            zptr2 = (uint32_t *) buf2;
            for (j=0; j<sb->blocksize/sizeof(uint32_t); j++)
            {
                if (read(fd, buf2, sb->blocksize) < 0) 
                {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                zone2 = zptr2[j];
                /* Print the zone */
                print_zone(zonesize, sb, j, lfirst, zone2, fd, &size_ctr, 
                        file->size, fdout);
            }

        }

    }

}

/* Verbose mode printing */
void print_verbose(struct superblock *sb, struct inode *cur)
{
    int i;
    char *dt;
    time_t mytime;
    char perms[11] = NO_PERMS;

    check_perms(cur, perms);


    
    printf("\nSuperblock Contents\nStored Fields:\n");
    printf("  ninodes:%13d\n", sb->ninodes);
    printf("  i_blocks:%12d\n", sb->i_blocks);
    printf("  z_blocks:%12d\n", sb->z_blocks);
    printf("  firstdata:%11d\n", sb->firstdata);
    printf("  log_zone_size:%7d", sb->log_zone_size);
    printf(" (zone size: %d)\n", sb->blocksize);
    printf("  max_file:%12d\n", sb->max_file);
    printf("  magic:%#15x\n", sb->magic);
    printf("  zones:%15d\n", sb->zones);
    printf("  blocksize:%11d\n", sb->blocksize);
    printf("  subversion:%10d\n", sb->subversion);

    printf("\nFile inode:\n");
    printf("  uint16_t  mode:%#13x  (%s)\n", cur->mode, perms);
    printf("  uint16_t links:%13d\n", cur->links);
    printf("  uint16_t uid:%15d\n", cur->uid);
    printf("  uint16_t gid:%15d\n", cur->gid);
    printf("  uint32_t size:%14d\n", cur->size);

    mytime = (time_t) cur->atime;
    dt = ctime(&mytime);
    printf("  uint32_t atime:%13d --- %s", cur->atime, dt);

    mytime = (time_t) cur->mtime;
    dt = ctime(&mytime);
    printf("  uint32_t mtime:%13d --- %s", cur->mtime, dt);
    
    mytime = (time_t) cur->ctime;
    dt = ctime(&mytime);
    printf("  uint32_t ctime:%13d --- %s", cur->ctime, dt);

    printf("\n  Direct zones:\n");
    for (i=0; i<DIRECT_ZONES; i++) 
    {
        printf("            zone[%d]  = %12d\n", i, cur->zone[i]);
    }
    printf("  uint32_t indirect:%15d\n", cur->indirect);
    printf("  uint32_t double:%17d\n", cur->two_indirect);
    
}

/********************************************/
/* List Utilities */
/*******************************************/

/* Prints out all nodes in linked list */
void print_list(node *head) 
{
    node *tmp;
    tmp = head;
    while(tmp)
    {
        printf("Entry: %s, inode: %d\n", tmp->entry->name,
                tmp->entry->inode);
        tmp = tmp->next;
    }
}

/* Free node list */
void free_list(node *head) 
{
    node *tmp_node;
    tmp_node = head;
    while (head) 
    {
        free(head->entry);
        tmp_node = head;
        head = head->next;
        free(tmp_node);
    } 
}


/* Adds a node to the linked list */
node *add_list(node *head, struct dirent *ptr) 
{

    struct dirent *entry;
    node *new_node;
    node *tmp;

    tmp = head;
    while (tmp && tmp->next)
    {
        tmp = tmp->next;
    }
    new_node = safe_malloc(sizeof(node));
    
    entry = safe_malloc(sizeof(struct dirent));
    memcpy(entry, ptr, sizeof(struct dirent));

    if (!head)
    {
        new_node->entry = entry;
        new_node->next = NULL;
        head = new_node;
        tmp = head;
    }
    else
    {
        tmp->next = new_node; 
        new_node->entry = entry;
        new_node->next = NULL;
        tmp = new_node;
    }
    return head;

}

void *safe_malloc(size_t size)
{
    void *outptr;

    if (!(outptr = malloc(size)))
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    return outptr;
}

