//
//  nyufile.c
//  nyufile
//
//  Created by Leslie Lu on 4/6/23.
//
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
extern char *optarg;
extern int optind,opterr;

int main(int argc, char * argv[]) {
    char  * optstr = "ilrRs";
    int opt;
    
    char * usg_info ="Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n";
    if(argc < 3 || argc > 6 || optind ==1){
        fprintf(stderr, usg_info);
        exit(EXIT_FAILURE);
    }
    while((opt=getopt(argc, argv, optstr))!=-1){
        
        switch(opt){
            
            case 'i':
                if (optind==2 || optind<argc){
//                    fprintf(stderr, "Print the file system information.");
                    fprintf(stderr, usg_info);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                if (optind==2 || optind<argc){
//                    fprintf(stderr, "List the root directory.");
                    fprintf(stderr, usg_info);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'r':
                if (optind==2 || optind==argc){
//                    fprintf(stderr, "Recover a contiguous file.");
                    fprintf(stderr, usg_info);
                    exit(EXIT_FAILURE);
                }
//                else{
//                    printf("inside r:%s\n",argv[optind]);
//                }
                break;
            case 'R':
                //printf("optarg:%s\n",optarg);
                if (optind==2 ||  argc != optind+3 || *argv[optind+2] != 's'){
                    fprintf(stderr, usg_info);
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
                    fprintf(stderr, usg_info);
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
