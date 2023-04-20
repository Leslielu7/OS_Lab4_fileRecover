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

#include "nyufile.h"

extern char *optarg;
extern int optind,opterr;

void print_filename(unsigned char * ptr, int isDir){
  //  int valid_filename = 0;
   // char * result = malloc(sizeof(char)*13);
//    int ptr_result = 0;
    for(int i=0;i<11;i++){
        
        if(ptr[i]!=0x20){
//            if (i<8){
//                valid_filename = 1;
//            }
//            if(i==8 && !valid_filename){
//                return 0;
//            }
            
            if(i==8 && !isDir){
//                *result = '.';
//                result++;
//                ptr_result++;
            printf(".");
            }
//            *result = ptr[i];
//            result++;
//            ptr_result++;
            printf("%c",ptr[i]);
        }
        
    }
////    *result = '\0';
////    ptr_result++;
//    result-=ptr_result;
//    printf("%s",result);
//    return 1;
}

//int isValid_filename(unsigned char * ptr//, int isDir
//                     ){
//    int valid_filename = 0;
////    char * result = malloc(sizeof(char)*13);
////    int ptr_result = 0;
//    for(int i=0;i<11;i++){
//
//        if(ptr[i]!=0x20){
//            if (i<8){
//                valid_filename = 1;
//            }
//            if(i==8 && !valid_filename){
//                return 0;
//            }
//
////            if(i==8 && !isDir){
////                *result = '.';
////                result++;
////                ptr_result++;
//////            printf(".");
////            }
////            *result = ptr[i];
////            result++;
////            ptr_result++;
//            //printf("%c",ptr[i]);
//        }
//
//    }
////    *result = '\0';
////    ptr_result++;
////    result-=ptr_result;
////    printf("%s",result);
//    return 1;
//}

int main(int argc, char * argv[]) {
    char  * optstr = "ilrRs";
    int opt;
   int fd;

    char * usg_info ="Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n";
    if(argc < 3 || argc > 6 //|| (getopt(argc, argv, optstr))==-1
       ){
        fprintf(stderr, "%s",usg_info);
        exit(EXIT_FAILURE);
        
    }

   //Open file
   // int fd = open(argv[1], O_RDONLY);
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
    //munmap(addr, sizeof(BootEntry));
    unsigned int fat_offset = boot_entry->BPB_BytsPerSec*boot_entry->BPB_RsvdSecCnt;
    int fat_size = boot_entry->BPB_BytsPerSec*boot_entry->BPB_NumFATs;
    int fat[fat_size];
    memcpy(fat,(unsigned char*)addr + fat_offset,fat_size);
    
    
    unsigned int data_area_offset = boot_entry->BPB_BytsPerSec*(boot_entry->BPB_RsvdSecCnt+boot_entry->BPB_NumFATs*boot_entry->BPB_FATSz32);
    
    DirEntry* dir_entries = (DirEntry*) ((unsigned char*)addr + data_area_offset);
    
    int cluster = boot_entry->BPB_RootClus; //cluster no. of root directory
    if(cluster!=2){ //rootcluster does not start at cluster 2
        dir_entries = (DirEntry*)
        ((unsigned char*)addr + (cluster-2) * boot_entry->BPB_BytsPerSec * boot_entry->BPB_SecPerClus);
    }
    
    if (dir_entries == NULL) {
        printf("Failed to initialize dir_entries\n");
        exit(EXIT_FAILURE);
    }
    
    opterr=0;
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
                    //printf("in l\n");
                    int num_entries = 0;
                    int num_entries_per_cluster = boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/sizeof(DirEntry);
                 
                    while(1){//iterate root directory entries in FAT
                        int cnt_entries = 0;
//                        printf("dir_entries->DIR_Name[0]:%c\n",dir_entries->DIR_Name[0]);
//                        printf("cluster:%d\n",cluster);
                        //start at cluster: root directory
                        while(cnt_entries<num_entries_per_cluster //&& dir_entries->DIR_Name[0]!=0
                              ){//iterate file & directory in this cluster
                            if(dir_entries->DIR_Attr==0x0F || //LFN
                               dir_entries->DIR_Name[0]==0 ||
                               dir_entries->DIR_Name[0]==0xe5 //|| !(isValid_filename(dir_entries->DIR_Name))
                               ){//skipped entries
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
                        
                        if(fat[cluster]>=0x0ffffff8){//the last cluster of root directory
                            break;
                        }
                        
                        cluster = fat[cluster];
                        if(fat[cluster]==0x00000000 || fat[cluster]==0x0ffffff7){//unallocated or bad cluster
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
                }else{
                    printf("in r\n");
                    printf("filename:%s\n",argv[optind]);
                    
                    char * targetfile = argv[optind];
                    
                    
                    
                    
                }
//                else{
//                    printf("inside r:%s\n",argv[optind]);
//                }
                break;
            case 'R':
                //printf("optarg:%s\n",optarg);
                if (optind!=3 ||  argc != optind+3 || strcmp(argv[optind+1],"-s")!=0){
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }
//                else if(opt =='s'// || (opt=getopt(argc, argv, optstr))!=-1
//                         ){
//                    if(optarg==NULL){
//                      printf("s is null\n");
////                        fprintf(stderr, "Recover a possibly non-contiguous file.");
////                        exit(EXIT_FAILURE);
//                    }
//                else{
//                    printf("this is Rs");
//                   // printf("optarg:%s\n",optarg);
//                }
                break;
            case 's':
                if (optind!=5 || optind==argc){
//                    fprintf(stderr, "Error: -s.");
                    fprintf(stderr, "%s",usg_info);
                    exit(EXIT_FAILURE);
                }
//                else{
//                    printf("inside s:%s\n",argv[optind]);
//                }
                break;
            
        }
        //printf("optarg:%s\n",optarg);
    }
}
