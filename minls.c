/* Name: Denis Pyryev (dpyryev)
 *       Aaron Vandergraaff (avanderg)
 *
 * Date: June 5, 2019
 * Assignment: Program 5
 * Instructor: Professor Phillip Nico
 * Course: CPE 453-03
 *
 * Description: This program lists a file or directory on the given
 *              filesystem (which should be a Minix filesystem). If the 
 *              optional path argument is given, it will traverse the
 *              filesystem to get to that path and list the directory entries
 *              or print the file information. The program also supports
 *              partitions and subpartitions, and will use the partitions in
 *              order to access Minix type filesystems properly. 
 */

#include "utilities.h"

int main(int argc, char *argv[])
{
    int fd, i, imagefile = 0, path = 0, inode_start;
    char buf[BUF_SIZE];
    int partition, subpartition, verbose;
    struct partition_table *pt;
    struct superblock *sb;
    struct inode cur, my_ino;
    node *head, *tmp_node;
    char perms[PERM_LEN] = NO_PERMS;
    char *mypath, *s, *str_ptr, *tmp;
    int str_i, ino_num, file_flag;
    uint32_t lfirst;

    /* Initialize variables */
    partition = EMPTY;
    subpartition = EMPTY;
    verbose = NOT_SET;
    file_flag = NOT_SET;

    /* Allocate the partition table */
    pt = safe_malloc(sizeof(struct partition_table));

    if (argc < NUM_ARGS) {
        printf("Not enough arguments\n");
        exit(EXIT_FAILURE);
    }

    /* Parse through the command line arguments */
    for(i=0; i < argc; i++)
    {
        /* Extract partition value */
        if (!strcmp("-p", argv[i]))
        {
            partition = (int) strtol(argv[++i], &tmp, DECIMAL);
            if (strlen(tmp) != 0) 
            {
                printf("Invalid partition\n");
                exit(EXIT_FAILURE);
            }

        }
        /* Extract subpartition value */
        else if (!strcmp("-s", argv[i])) 
        { 
            if (partition == EMPTY) 
            {
                printf("Invalid subpartition: no partition\n");
                exit(EXIT_FAILURE);
            }
            subpartition = (int) strtol(argv[++i], &tmp, 10);
            if (strlen(tmp) != 0) 
            {
                printf("Invalid partition\n");
                exit(EXIT_FAILURE);
            }
        }
        
        /* Set the verbose flag if -v is given */
        else if (!strcmp("-v", argv[i]) != 0)
        {
            verbose = i;
        }
        
        /* After passing through -p, -s, and -v, the next thing will be the
         * imagefile in the command line. */
        else if (!imagefile) 
        {
            imagefile = i;
        }

        /* If everything but one thing is taken from command line, the last
         * thing is the path */
        else
        {
            path = i;
        }
    }

    /* Open the imagefile that was parsed from the command line */
    if ((fd = open(argv[imagefile], O_RDONLY)) < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* Read the first 2048 bytes of data from the imagefile into buf */
    if ((read(fd, buf, BUF_SIZE)) < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }

    /* Extract the partition table and superblock from the first 2048 bytes */
    lfirst = check_partition_table(fd, pt, buf, partition, subpartition);
    sb = (struct superblock *)&buf[SB_OFFSET];
    
    /* Check for Minix filesystem in superblock */
    if (sb->magic != MINIX_SBMAGIC) 
    {
        printf("This is not a Minix filesystem\n");
        exit(EXIT_FAILURE);
    }

    /* Seek to the start of the partition */
    if ((lseek(fd, lfirst, SEEK_SET)) < 0)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    read_inode_table(&cur, fd, sb, lfirst);
   
    /* If user didn't supply a path, use root '/' */
    if (!path)
    {
        printf("/:\n");
        head = read_path_minls(&cur, sb, fd, "/", "/", &file_flag, lfirst); 
    }
    /* Use the path that the user supplied */
    else
    {
        mypath = safe_malloc(strlen(argv[path])+1);
        strcpy(mypath, argv[path]);

        /* Return a linked list of dirent structs from the path given */
        head = read_path_minls(&cur, sb, fd, mypath, argv[path], &file_flag, 
                lfirst); 
        if (!file_flag)
        {
            printf("%s:\n", argv[path]);
        }
        free(mypath);
    }

    tmp_node = head;
    
    /* Traverse the linked list starting at head, and print out the name
     * of each dirent for the files that exist in the path directory */
    while (tmp_node && !file_flag)
    {
        /* Seek to the start of the inode table */
        inode_start = lfirst * SECTOR_SIZE + 
            sb->blocksize * (INO_OFFSET + sb->i_blocks + sb->z_blocks);
        if (lseek(fd, inode_start + sizeof(struct inode) * 
                        (tmp_node->entry->inode - 1), SEEK_SET) < 0) 
        {
            perror("lseek");
            exit(EXIT_FAILURE);
        }
        /* Read in the seeked inode into cur */
        if (read(fd, &cur, sizeof(struct inode)) < 0) 
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        /* Extract the inode permissions */
        check_perms(&cur, perms);
        /* Print the file/directory information */
        printf("%s%10d %s\n", perms, cur.size, tmp_node->entry->name);
        reset_perms(perms);
        tmp_node = tmp_node->next;
        
    }

    /* If the verbose mode is set, print out the relevant information */
    if (verbose)
    {
        /* Parse through all of the leading and trailing '/' characters
         * within the path given */
        str_i = strlen(argv[path]);
        if (argv[path][strlen(argv[path])-1] == '/')
        {
            str_i -= SLASH_OFFSET;
            argv[path][str_i+1] = '\0';
        }
        while (argv[path][str_i] != '/')
        {
            str_i--;

        }
        if (!path) 
        {
            str_ptr = "/";
        }
        else 
        {
            str_ptr = argv[path] + str_i + 1;
        }

        /* Traverse through the linked list until you find the correct name
         * of the inode to print */
        tmp_node = head;
        while (tmp_node)
        {
            s = (char *) tmp_node->entry->name;
            /* Compare names */
            if ((file_flag && !strcmp(s, str_ptr)) || 
                    (!file_flag && !strcmp(s, "."))) 
            {
                ino_num  = tmp_node->entry->inode;
                break;
            }
            tmp_node = tmp_node->next;
        }
        /* Once the dirent within the linked list was confirmed, extract
         * that dirent's inode and print its information */
        get_ino(ino_num, sb, &my_ino, fd, lfirst);
        print_verbose(sb, &my_ino);
    }

    /* Free everything allocated and close the filesystem image file */
    free_list(head);
    free(pt);
    close(fd);

    return 0;
}
