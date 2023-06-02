#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

typedef struct _FileData{
    char* dir;
    off_t filesize;
    int match;
    int done;
}FileData;

typedef struct _Node{
    FileData* fd;
    struct _Node* next;
    struct _Node* same;
}Node;

typedef struct argv_t{
    int t_value;
    int ist;
    int m_value;
    int ism;
    char* o_value;
    int iso;
    int opt;
}argv_t;

bool compare_files(const char* file_path1, const char* file_path2);
argv_t get_argv(int argc, char *argv[]);

argv_t arg;
int current = 0;
pthread_mutex_t match_lock; 
pthread_mutex_t link_lock; 
pthread_mutex_t subtasks_lock ;

FileData** filePath;

volatile sig_atomic_t g_running = 0;
int count;
int match_count;
pthread_t** threads;

int node_count;
Node* head;
Node* tail;

void update_match(FileData *fd1, FileData *fd2){
    if(fd1->match == 0){
        if(fd2->match == 0){
            fd1->match = match_count;
            fd2->match = match_count;
            match_count++;
        }else{
            fd1->match = fd2->match;
        }
    }
    else if(fd2->match == 0){
        fd2->match = fd1->match;
    }
    else{
        if(fd1->match > fd2->match){
            fd1->match = fd2->match;
        }
        else{
            fd2->match = fd1->match;
        }
    }
}

int find_sameFile(FileData * fd){
    // check
    for(int i=0; i<count; i++){
        if(g_running == 1) exit(0);
        if(filePath[i]->done == 1) continue;
        if(filePath[i]->dir == fd->dir) continue;
        if(filePath[i]->filesize != fd->filesize) continue;

        // match
        if(compare_files(fd->dir,filePath[i]->dir)){
            pthread_mutex_lock(&match_lock);
            update_match(fd,filePath[i]);
            pthread_mutex_unlock(&match_lock);
        }
    }
    
    return 0;
}

Node* link_sameFile(int match_value){
    // printf("<<link_sameFile %d",match_value);
    Node * match_node = malloc(sizeof(Node));
    match_node->fd = NULL;
    match_node->next = NULL;
    match_node->same = NULL;
    Node * same_tail = match_node;
    for(int i=0;i<count; i++){
        if(filePath[i]->match == match_value){
            same_tail->same = malloc(sizeof(Node));
            same_tail = same_tail->same;
            same_tail->fd = filePath[i];
            same_tail->next = NULL;
            same_tail->same = NULL;
        }
    }
    if(match_node->same == NULL) {
        free(match_node);
        return NULL;
    }
    match_node = match_node->same;
    return match_node;
}

void processDirectory(const char* path, FileData*** filePath, int* count) {
    DIR* dir;
    struct dirent* entry;
    struct stat fileStat;

    // Open directory
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Create the full path to the entry
        char entryPath[1024];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", path, entry->d_name);

        // Get file/directory information
        if (stat(entryPath, &fileStat) == -1) {
            perror("stat");
            continue;
        }

        // Check if the entry is a regular file
        if (S_ISREG(fileStat.st_mode)) {
            //smaller than minimum
            if(fileStat.st_size < arg.m_value){
                continue;
            }
            // Save the file path to the array
            *filePath = realloc(*filePath, (*count + 1) * sizeof(FileData*));
            FileData* temp_FileData = malloc(sizeof(FileData));
            temp_FileData->dir = strdup(entryPath);
            temp_FileData->filesize = fileStat.st_size;
            temp_FileData->done = 0;
            temp_FileData->match = 0;
            (*filePath)[*count] = temp_FileData;
            (*count)++;
        }
        else if (S_ISDIR(fileStat.st_mode)){
            processDirectory(entryPath, filePath, count);
        }
        // Skip directories and other file types
        else {
            continue;
        }
    }

    // Close directory
    closedir(dir);
}

FileData * get_File () 
{
	FileData * s ;
	pthread_mutex_lock(&subtasks_lock) ;
		if(current >= count) s = NULL;
        else s = filePath[current] ;
		current++;
	pthread_mutex_unlock(&subtasks_lock) ;

	return s ;
}

void * worker (void * arg)
{
	FileData * fd ;

	while ((fd = get_File()) != NULL) {
		find_sameFile(fd) ;
	}
    if(fd != NULL) fd->done = 1;
	return NULL ;
}

int get_Node () 
{
	int s ;
	pthread_mutex_lock(&subtasks_lock) ;
		if(match_count <= node_count) s = -1;
        else s = node_count;
		node_count++;
	pthread_mutex_unlock(&subtasks_lock) ;

	return s ;
}

void * link_work(void * arg)
{
	Node * node ;
    int idx;
	while ((idx = get_Node()) != -1) {
        if((node = link_sameFile(idx)) == NULL){
            continue; 
        }
        pthread_mutex_lock(&link_lock) ;
        tail->next = node;
        tail = tail->next;
        pthread_mutex_unlock(&link_lock) ;
	}
	return NULL ;
}

void print_ll(){
    Node* temp_next = head->next;
    Node* temp_same;
 
    if(arg.iso != 0){
        FILE *file;
        file = fopen(arg.o_value, "w");
        if (file == NULL) {
            printf("Can not open output file.\n");
            return; 
        }
        fprintf(file,"[\n");
        while (temp_next != NULL)
        {
            if (temp_next->same == NULL)
                continue;
            fprintf(file,"\t[\n");
            temp_same = temp_next;
            while (temp_same != NULL)
            {
                fprintf(file,"\t\t%s\n", temp_same->fd->dir);
                temp_same = temp_same->same;
            }
            temp_next = temp_next->next;
            fprintf(file,"\t]");
            if (temp_next != NULL)
                fprintf(file,",\n");
            else
                fprintf(file,"\n");
        }
        fprintf(file,"]\n");
        fclose(file);
    }
    else
    {
        printf("[\n");
        while (temp_next != NULL)
        {
            if (temp_next->same == NULL)
                continue;
            printf("\t[\n");
            temp_same = temp_next;
            while (temp_same != NULL)
            {
                printf("\t\t%s\n", temp_same->fd->dir);
                temp_same = temp_same->same;
            }
            temp_next = temp_next->next;
            printf("\t]");
            if (temp_next != NULL)
                printf(",\n");
            else
                printf("\n");
        }
        printf("]\n");
    }
}

void sigintHandler(int signum) {
    g_running = 0; 
    for (int i = 0 ; i < arg.t_value; i++) {
		pthread_join(*(threads[i]), NULL) ;
	}
    
    for (int i = 0 ; i < arg.t_value; i++) {
		pthread_create((threads[i]), NULL, link_work, NULL) ;
	}
    for (int i = 0 ; i < arg.t_value; i++) {
		pthread_join(*(threads[i]), NULL) ;
	}
    print_ll();
    
    exit(0);
}

int main(int argc, char *argv[]){
    int i;
    // parse options
    arg = get_argv(argc,argv);
    printf("t: %d , m: %d , o: %s\n",arg.t_value, arg.m_value, arg.o_value);    
    const char* path = argv[argc-1];
    // Initialize array to store file paths
    pthread_mutex_init(&match_lock, NULL) ;
    pthread_mutex_init(&link_lock, NULL) ;
    pthread_mutex_init(&subtasks_lock, NULL) ;
    filePath = NULL;
    threads = malloc(arg.t_value * sizeof(pthread_t*));
    count = 0;
    match_count = 1;

    


    // Process the directory and its contents
    processDirectory(path, &filePath, &count);

    // control exception
    signal(SIGINT, sigintHandler);

    // make number of t threads
    for (i = 0 ; i < arg.t_value; i++) {
        threads[i] = malloc(sizeof(pthread_t));
		pthread_create((threads[i]), NULL, worker, NULL) ;
	}
    for (i = 0 ; i < arg.t_value; i++) {
		pthread_join(*(threads[i]), NULL) ;
	}

    node_count = 1;
    head = malloc(sizeof(Node*));
    head->fd = NULL;
    head->next = NULL;
    head->same = NULL;
    tail = head;

    // ctrl+c restore
    signal(SIGINT, SIG_DFL);
    for (int i = 0 ; i < arg.t_value; i++) {
		pthread_create((threads[i]), NULL, link_work, NULL) ;
	}
    for (int i = 0 ; i < arg.t_value; i++) {
		pthread_join(*(threads[i]), NULL) ;
	}

    print_ll();

    // Free memory allocated for file paths
    for (int i = 0; i < count; i++) {
        free(filePath[i]);
    }
    free(filePath);

    return 0;
}

bool compare_files(const char* file_path1, const char* file_path2) {
    if(strcmp(file_path1,file_path2) == 0) return false;
    FILE* file1 = fopen(file_path1, "rb");
    FILE* file2 = fopen(file_path2, "rb");

    if (file1 == NULL || file2 == NULL) {
        // Open failur
        if (file1 != NULL) {
            fclose(file1);
        }
        if (file2 != NULL) {
            fclose(file2);
        }
        printf("Can't open file\n");
        return false;
    }

    bool are_equal = true;
    int ch1, ch2;

    while ((ch1 = fgetc(file1)) != EOF & (ch2 = fgetc(file2)) != EOF) {
        if (ch1 != ch2) {
            are_equal = false;
            break;
        }
    }

    // Check EOF
    if (ch1 == EOF & ch2 != EOF || ch1 != EOF & ch2 == EOF) {
        are_equal = false;
    }

    fclose(file1);
    fclose(file2);

    return are_equal;
}

argv_t get_argv(int argc, char *argv[]){
    argv_t arg;
    arg.t_value = -1;
    arg.m_value = 1024;
    arg.ist = 0;
    arg.ism = 0;
    arg.iso = 0;
    arg.o_value = NULL;


    // Parsing command line arguments
    while ((arg.opt = getopt(argc, argv, "t:m:o:")) != -1) {
        switch (arg.opt) {
            case 't':
                arg.ist = 1;
                arg.t_value = atoi(strchr(optarg, '=') + 1);
                break;
            case 'm':
                arg.ism = 1;
                arg.m_value = atoi(strchr(optarg, '=') + 1);
                break;
            case 'o':
                arg.iso = 1;
                arg.o_value = strchr(optarg, '=') + 1;
                break;
            case '?':
                if (arg.ist == 0){
                    printf("Missing argument(s)!\n");
                    printf("usage: ./findeq -t=\"NUM\" -m=\"NUM\" -o=\"FILE\" DIR\n");
                }
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                abort();
            default:
                abort();
        }
    }
    if(arg.t_value > 64){
        printf("t can not be over 64, t is set to 64\n");
        arg.t_value = 64;
    }
    if(argc - arg.ism - arg.iso - arg.ist != 2){
        printf("Argument Error!\n");
        printf("usage: ./findeq -t=\"NUM\" -m=\"NUM\" -o=\"FILE\" DIR\n");
        abort();
    }

    return arg;
}