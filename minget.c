/* Name: Denis Pyryev (dpyryev)
 *
 * Date: May 3, 2019
 * Assignment: Program 3
 * Instructor: Professor Phillip Nico
 * Course: CPE 453-03
 *
 * Description:
 */

#include "utilities.h"




int main(int argc, char *argv[])
{
    int fd, fdout, i, imagefile;
    char buf[BUF_SIZE];
    char *tmp;
    int partition, subpartition;
    struct partition_table *pt;
    struct superblock *sb;
    struct inode cur;
    int verbose;
    node *head;
    node *tmp_node;
    char *mypath;
    int path;
    char *s;
    int str_i;
    struct inode my_ino;
    int ino_num;
    char *str_ptr;
    int file_flag;
    uint32_t lfirst;

    path = 0;
    imagefile = 0;
    partition = -1;
    subpartition = -1;
    verbose = 0;
    file_flag = 0;
    fdout = STDOUT_FILENO;

    pt = malloc(sizeof(struct partition_table));

    if (argc < 2) {
        printf("Not enough arguments\n");
        exit(EXIT_FAILURE);
    }


    for(i=0; i < argc; i++)
    {
        if (!strcmp("-p", argv[i]))
        {
            partition = (int) strtol(argv[++i], &tmp, 10);
            if (strlen(tmp) != 0) 
            {
                printf("Invalid partition");
                exit(EXIT_FAILURE);
            }

        }
        else if (!strcmp("-s", argv[i])) 
        { 
            if (partition == -1) 
            {
                printf("Invalid supartition: no partition\n");
                exit(EXIT_FAILURE);
            }
            subpartition = (int) strtol(argv[++i], &tmp, 10);
            if (strlen(tmp) != 0) 
            {
                printf("Invalid partition");
                exit(EXIT_FAILURE);
            }
            
        }

        else if(!strcmp("-v", argv[i]) != 0)
        {
            verbose = i;
        }
        else if (!imagefile) 
        {
            imagefile = i;
        }
        else if (!path)
        {
            path = i;
        }
        else 
        {
            if ((fdout = open(argv[i], O_CREAT | O_TRUNC | O_WRONLY,
                            S_IWUSR | S_IRUSR)) < 0)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
        }
    }

    if ((fd = open(argv[imagefile], O_RDONLY)) < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }


    /* Does there have to be a partition table? */

    if ((read(fd, buf, BUF_SIZE)) < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    lfirst = check_partition_table(fd, pt, buf, partition, subpartition);

    /* printf("lfirst: %d\n", lfirst);*/
    sb = (struct superblock *)&buf[SB_OFFSET];
    
    /*printf("sb->magic: %x\n", sb->magic);*/
    if (sb->magic != MINIX_SBMAGIC) 
    {
        /*printf("sb->magic: %x\n", sb->magic);*/
        printf("This is not a Minix filesystem\n");
        exit(EXIT_FAILURE);
    }

    lseek(fd, lfirst, SEEK_SET);
    


    read_inode_table(&cur, fd, sb, lfirst);
    
    if (!path)
    {
        /*head = read_root(&cur, sb, fd);*/
        /*printf("reading root\n");*/
        printf("Provide a file\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        /*printf("path passed: %s\n", mypath);*/
        /*printf("%s:\n", argv[path]); */
        mypath = malloc(strlen(argv[path])+1);
        strcpy(mypath, argv[path]);
        head = read_path_minget(&cur, sb, fd, mypath, argv[path], &file_flag, 
                lfirst, fdout); 
        free(mypath);
    }

    if (verbose)
    {
        str_i = strlen(argv[path]);
        if (argv[path][strlen(argv[path])-1] == '/')
        {
            str_i -= 2;
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
       /* printf("str_ptr: %s\n", str_ptr);*/

        tmp_node = head;
        while (tmp_node)
        {
            s = (char *) tmp_node->entry->name;
            if ((file_flag && !strcmp(s, str_ptr)) || 
                    (!file_flag && !strcmp(s, "."))) 
            {
                ino_num  = tmp_node->entry->inode;
                break;
            }
            tmp_node = tmp_node->next;
        }
        /*printf("Entry Name: %s\n", tmp_node->entry->name);
        printf("Entry Ino Num: %d\n", tmp_node->entry->inode);
        */
        get_ino(ino_num, sb, &my_ino, fd, lfirst);
        print_verbose(sb, &my_ino);
    }

    free_list(head);
    free(pt);
    close(fd);
    return 0;
}
