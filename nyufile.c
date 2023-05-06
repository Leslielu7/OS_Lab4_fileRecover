//
//  nyufile.c
//  nyufile
//
//  Created by Leslie Lu on 4/6/23.
//
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "nyufile.h"

#include <openssl/sha.h>
#define SHA_DIGEST_LENGTH 20

extern char *optarg;
extern int optind,opterr;

void print_filename(unsigned char * ptr, int isDir);
char * return_filename (unsigned char * ptr);

int main(int argc, char * argv[]) {
    char  * optstr = "ilrRs";
    int opt;
    int fd;
    int is_r = 0;
    int is_R = 0;
  

    char * usg_info ="Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n";
    if(argc < 3 || argc > 6
       ){
        fprintf(stderr, "%s",usg_info);
        exit(EXIT_FAILURE);
        
    }

   //Open file
    if ((fd = open(argv[1], O_RDWR)) == -1){
        //printf("Error open file: %s\n",argv[1]);
        fprintf(stderr, "%s",usg_info);
        exit(EXIT_FAILURE);
    }

    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1){
        printf("fstat failed:%s\n",argv[1]);
    }

    // Map file into memory
    void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED){
        printf("MAP_FAILED:%s\n",argv[1]);
    }
    
    BootEntry* boot_entry = (BootEntry*) addr;
   
    unsigned int fat_offset = boot_entry->BPB_BytsPerSec * boot_entry->BPB_RsvdSecCnt;
  
    int *fat_ptr = (int*)((unsigned char*)addr + fat_offset);
    
    unsigned int data_area_offset = boot_entry->BPB_BytsPerSec * (boot_entry->BPB_RsvdSecCnt + boot_entry->BPB_NumFATs * boot_entry->BPB_FATSz32);
 

    
    //cluster no. of root directory
    unsigned int cluster = boot_entry->BPB_RootClus;

        DirEntry*  dir_entries = (DirEntry*)
        ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);

    
    if (dir_entries == NULL) {
        printf("Failed to initialize dir_entries\n");
        exit(EXIT_FAILURE);
    }
    
    opterr=0;
    //int s_opt = 0;
    char * targetfile = malloc(sizeof(char)*13);
    while((opt=getopt(argc, argv, optstr))!=-1){
        
        switch(opt){
            case '?':
                fprintf(stderr, "%s",usg_info);
                exit(EXIT_FAILURE);
                break;
            case 'i':
                if (optind!=3 || optind<argc){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }else{
                    printf("Number of FATs = %d\n", boot_entry->BPB_NumFATs);
                    printf("Number of bytes per sector = %d\n", boot_entry->BPB_BytsPerSec);
                    printf("Number of sectors per cluster = %d\n", boot_entry->BPB_SecPerClus);
                    printf("Number of reserved sectors = %d\n", boot_entry->BPB_RsvdSecCnt);
                }
               break;
            case 'l':
                if (optind!=3 || optind<argc){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }else{
                    int num_entries = 0;
                    int num_entries_per_cluster = boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/sizeof(DirEntry);
                 
                    while(1){//iterate root directory entries in FAT
                        int cnt_entries = 0;

                        //start at cluster: root directory
                        while(cnt_entries<num_entries_per_cluster ){//iterate file & directory in this cluster
                            if(dir_entries->DIR_Attr==0x0F || //LFN
                               dir_entries->DIR_Name[0]==0 ||
                               dir_entries->DIR_Name[0]==0xe5 ){//skipped entries
                                dir_entries++;
                                cnt_entries++;
                                continue;
                            }
                            else if(dir_entries->DIR_Attr==0x20){ //file
                                if(dir_entries->DIR_FileSize!=0){ //file: not empty
                                    print_filename(dir_entries->DIR_Name,0);
                                    printf(" (size = %d, starting cluster = %d)\n"
                                           ,dir_entries->DIR_FileSize,65536*dir_entries->DIR_FstClusHI + dir_entries->DIR_FstClusLO);
                                }else{
                                    print_filename(dir_entries->DIR_Name,0);
                                    printf(" (size = %d)\n" ,dir_entries->DIR_FileSize);
                                }
                                
                            }else if(dir_entries->DIR_Attr==0x10){ //directory
                                print_filename(dir_entries->DIR_Name,1);
                                printf("/ (starting cluster = %d)\n"
                                       ,65536*dir_entries->DIR_FstClusHI + dir_entries->DIR_FstClusLO);
                            }
                            dir_entries++;
                            num_entries++;
                            cnt_entries++;
                         }
                        if(*(fat_ptr + cluster) >=0x0ffffff8){//the last cluster of root directory
                            break;
                        }
                        
                        cluster = *(fat_ptr + cluster);
            
                        if(*(fat_ptr + cluster)==0x00000000 || *(fat_ptr + cluster)==0x0ffffff7){//unallocated or bad cluster
                            printf("unallocated or bad cluster\n");
                            break;
                        }
           
                        
                        dir_entries = (DirEntry*) ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
                        }
                        
                        printf("Total number of entries = %d\n",num_entries);
                }
               break;
                
            case 'r':
                if (optind!=3 || optind==argc || argc == optind+2 || (argc == optind+3 && strcmp(argv[optind+1],"-s")!=0)){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }
                else{
                    
                    if(argc == optind+3 && strcmp(argv[optind+1],"-s")==0){
                        targetfile = argv[optind];
                        is_r = 1;
                        break;
                    }
               
                    targetfile = argv[optind];
                    int num_entries_per_cluster = boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/sizeof(DirEntry);
                 
                    int done = 0;//found at least one filename match
                    int mul = 0;// found multiple filename match
                    unsigned int recover_dir_entry ;// dir entry of the first filename match
                    
                    while(1){//iterate root directory entries in FAT
                        int cnt_entries = 0;
                        
                        while(cnt_entries<num_entries_per_cluster){//iterate file & directory in this cluster
                            if( dir_entries->DIR_Name[0]!=0xe5 ){//skipped entries
                                dir_entries++;
                                cnt_entries++;
                                continue;
                            }
                            else if(dir_entries->DIR_Attr==0x20){ //file
                                char * try = return_filename(dir_entries->DIR_Name);
                                int i=1;
                                while(targetfile[i] == try[i]){
                                    i++;
                                }
                                if(i==(int)strlen(targetfile)+1){//filename matched
                                    if(done){
                                        printf("%s: multiple candidates found\n",targetfile);
                                        mul = 1;
                                        exit(EXIT_SUCCESS);
                                    }
                                    
                                    recover_dir_entry = cnt_entries;
                                    done = 1;
                                }
                            }
                            dir_entries++;
                            cnt_entries++;
                         }
                        
                        if(*(fat_ptr + cluster) >=0x0ffffff8){//the last cluster of root directory
                            break;
                        }
                        
                        cluster = *(fat_ptr + cluster);// move to next cluster
                        dir_entries = (DirEntry*) ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
                        }
                    
                    if(!done){
                        printf("%s: file not found\n",targetfile);
                    }else if (!mul){//mul=0
                        dir_entries = (DirEntry*)
                        ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
                     
                        dir_entries += recover_dir_entry;
                        
       
                        dir_entries->DIR_Name[0] = targetfile[0];
                    
                        unsigned int filesize = dir_entries->DIR_FileSize;
                        unsigned int clusterbyte = boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus;
                        unsigned int index = 65536*dir_entries->DIR_FstClusHI + dir_entries->DIR_FstClusLO;
                       
                        //BPB_FATSz32: the size of each FAT in terms of the number of sectors.
                        unsigned int size_per_fat = boot_entry->BPB_BytsPerSec * boot_entry->BPB_FATSz32;
                        
                        if(filesize<=clusterbyte && filesize!=0){
                            
                            
                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//iterate all FATs, single fat size = single sector size
                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                              }
                        }
                        else if(filesize!=0){  // filesize > clusterbyte
                            while(filesize>clusterbyte){
                                for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                    *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = index+1;
                                }
                                index++;
                                filesize-=clusterbyte;
                            }
                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                            }
                        }
                        printf("%s: successfully recovered\n",targetfile);
                    }
                }
                

                break;
            case 'R':
                //printf("optarg:%s\n",optarg);
                if (optind!=3 ||  argc != optind+3 || strcmp(argv[optind+1],"-s")!=0){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }
                else {
                    if(argc == optind+3 && strcmp(argv[optind+1],"-s")==0){
                        targetfile = argv[optind];
                        is_r = 1;
                        break;
                    }
                }
                break;

            case 's':
             
                if (optind!=5 || optind==argc){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }
                else if(is_r == 1){
                    char * checksum = argv[optind];//checksum argument after -s
        //start
                    int num_entries_per_cluster = boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/sizeof(DirEntry);
                    int done = 0;//found at least one filename match
                    while(1){//iterate root directory entries in FAT
                        int cnt_entries = 0;
                        while(cnt_entries<num_entries_per_cluster){//iterate file & directory in this cluster
                            if( dir_entries->DIR_Name[0]!=0xe5 ){//skipped entries
                                dir_entries++;
                                cnt_entries++;
                                continue;
                            }
                            else if(dir_entries->DIR_Attr==0x20){ //file
                                char * try = return_filename(dir_entries->DIR_Name);
                                int i=1;
                                while(targetfile[i] == try[i]){
                                    i++;
                                }
                                if(i==(int)strlen(targetfile)+1){//filename matched
                                    
                                    // get this file's content cluster
                                    unsigned int index = 65536*dir_entries->DIR_FstClusHI + dir_entries->DIR_FstClusLO;
                                    unsigned int clusterbyte = boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus;
                                    // checksum to check content, continuously
                                    unsigned char* cluster_entries = (unsigned char*)addr + data_area_offset + (index-2)*clusterbyte;
                                    unsigned int filesize = dir_entries->DIR_FileSize;
                                    unsigned char * md = malloc(SHA_DIGEST_LENGTH);
                                    unsigned char * file_content = SHA1(cluster_entries,filesize,md);
           
                                    if(file_content == NULL){
                                        printf("file_content failed\n");
                                    }
                                    
                                    int i=0;
                                    while(i<SHA_DIGEST_LENGTH){
                                        char str[3];
                                        sprintf(str, "%02x", md[i]);
                                        if(str[0]!=checksum[i*2] || str[1]!=checksum[i*2+1]){
                                            break;
                                        }
                                        i++;
                                    }
                                    if(i==SHA_DIGEST_LENGTH){//checksum verified

                                        dir_entries->DIR_Name[0] = targetfile[0];
                                       
                                        //BPB_FATSz32: the size of each FAT in terms of the number of sectors.
                                        int size_per_fat = boot_entry->BPB_BytsPerSec * boot_entry->BPB_FATSz32;
                                    
                                        if(filesize<=clusterbyte && filesize!=0){
                                           
                                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//iterate all FATs, single fat size = single sector size
                                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                                              }
                                        }
                                        else{  // filesize > clusterbyte
                                            while(filesize>clusterbyte){
                                                for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                                    *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = index+1;
                                                }
                                                index++;
                                                filesize-=clusterbyte;
                                            }
                                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                                            }
                                        }
                                        printf("%s: successfully recovered with SHA-1\n",targetfile);
                                        done = 1;
                                        break;
                                    }
                                    //if not same, try next file
                                }
                            }
                            dir_entries++;
                            cnt_entries++;
                         }
                        if(*(fat_ptr + cluster) >=0x0ffffff8){//the last cluster of root directory
                            break;
                        }
                        cluster = *(fat_ptr + cluster);// move to next cluster
                        dir_entries = (DirEntry*) ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
                        }
                    
                    if(!done){
                        printf("%s: file not found\n",targetfile);
                    }
                }else if(is_R == 1){
                    //is_R
                    printf("this is R");
                    char * checksum = argv[optind];//checksum argument after -s
        //start
                    int num_entries_per_cluster = boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/sizeof(DirEntry);
                    int done = 0;//found at least one filename match
                    while(1){//iterate root directory entries in FAT
                        int cnt_entries = 0;
                        while(cnt_entries<num_entries_per_cluster){//iterate file & directory in this cluster
                            if( dir_entries->DIR_Name[0]!=0xe5 ){//skipped entries
                                dir_entries++;
                                cnt_entries++;
                                continue;
                            }
                            else if(dir_entries->DIR_Attr==0x20){ //file
                                char * try = return_filename(dir_entries->DIR_Name);
                                int i=1;
                                while(targetfile[i] == try[i]){
                                    i++;
                                }
                                if(i==(int)strlen(targetfile)+1){//filename matched
                                    
                                    // get this file's content cluster
                                    unsigned int index = 65536*dir_entries->DIR_FstClusHI + dir_entries->DIR_FstClusLO;
                                    unsigned int clusterbyte = boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus;
                                    // checksum to check content, uncontinuously
                                    
                                    unsigned char* cluster_entries = (unsigned char*)addr + data_area_offset + (index-2)*clusterbyte;
                                    unsigned int filesize = dir_entries->DIR_FileSize;
                                    unsigned char * md = malloc(SHA_DIGEST_LENGTH);
                                    unsigned int num_cluster = (filesize%clusterbyte == 0)? filesize%clusterbyte:filesize%clusterbyte+1;
                                    printf("filesize:%d",filesize);
                                    printf("clusterbyte:%d",clusterbyte);
                                    printf("num_cluster:%d",num_cluster);
                                 
                                    
                                    unsigned char * file_content = SHA1(cluster_entries,filesize,md);
           
                                    if(file_content == NULL){
                                        printf("file_content failed\n");
                                    }
                                    
                                    int i=0;
                                    while(i<SHA_DIGEST_LENGTH){
                                        char str[3];
                                        sprintf(str, "%02x", md[i]);
                                        if(str[0]!=checksum[i*2] || str[1]!=checksum[i*2+1]){
                                            break;
                                        }
                                        i++;
                                    }
                                    if(i==SHA_DIGEST_LENGTH){//checksum verified
//
                                        dir_entries->DIR_Name[0] = targetfile[0];
                                       
                                        //BPB_FATSz32: the size of each FAT in terms of the number of sectors.
                                        int size_per_fat = boot_entry->BPB_BytsPerSec * boot_entry->BPB_FATSz32;
                                    
                                        if(filesize<=clusterbyte && filesize!=0){
                                            
                                            
                                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//iterate all FATs, single fat size = single sector size
                                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                                              }
                                        }
                                        else{  // filesize > clusterbyte
                                            while(filesize>clusterbyte){
                                                for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                                    *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = index+1;
                                                }
                                                index++;
                                                filesize-=clusterbyte;
                                            }
                                            for(int i=0;i<boot_entry->BPB_NumFATs;i++){//update the same no. of cluster for each FAT
                                                *((int *)((unsigned char *)fat_ptr + (index*sizeof(int)+i*size_per_fat))) = 0x0FFFFFF8;
                                            }
                                        }
                                        printf("%s: successfully recovered with SHA-1\n",targetfile);
                                        done = 1;
                                        break;
                                    }
                                    //if not same, try next file
                                }
                            }
                            dir_entries++;
                            cnt_entries++;
                         }
                        if(*(fat_ptr + cluster) >=0x0ffffff8){//the last cluster of root directory
                            break;
                        }
                        cluster = *(fat_ptr + cluster);// move to next cluster
                        dir_entries = (DirEntry*) ((unsigned char*)addr + data_area_offset + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
                        }
                    
                    if(!done){
                        printf("%s: file not found\n",targetfile);
                    }
                    
                }
            break;
        }
       
    }
    
}

void print_filename(unsigned char * ptr, int isDir){
    for(int i=0;i<11;i++){
        
        if(ptr[i]!=0x20){

            if(i==8 && !isDir){
            printf(".");
            }

            printf("%c",ptr[i]);
        }
    }
}

char * return_filename (unsigned char * ptr){
    static char result[13] ;
    int result_ptr = 0;
    for(int i=0;i<11;i++){

        if(ptr[i]!=0x20){

            if(i==8){
                result[result_ptr] = '.';
                result_ptr++;
                
            }
            result[result_ptr] = ptr[i];
            result_ptr++;
            
        }
    }
    result[result_ptr] = '\0';
   
    return result;
}
