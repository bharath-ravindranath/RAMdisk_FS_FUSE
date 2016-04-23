#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define FILE_DATA ((struct file_info *) fuse_get_context()->private_data)

struct file_info {
	char name[256];
	char parent[512];
	int type; // 0 = directory 1 = regualr file
	struct file_info *next;
	int file_size;
	mode_t mode;
};

int line=0;


char * getparent(struct file_info *head, char name[]){
	int i=0;
	for(i =strlen(name)-1; i >= 0 && name[i] != '/'; i--) ;
	if(i == 0) i++;

	while(head != NULL && strncmp(head->name, name, i)) head = head->next;
	if(head == NULL) return NULL;
	return head->name;
}

int remove_from_file_info(struct file_info *head, char name[]){

	printf("%d remove_from_file_info \n", line++);
	struct file_info *temp1, *temp2;
	temp1 = temp2 = head;
	
	while(temp1 != NULL && strcmp(temp1->name, name)){
		temp2 = temp1;
		temp1 = temp1->next;
	}

	if(temp1 == NULL) return ENOENT;
	
	else if(temp1->type != 0){
		return ENOTDIR;
	}
	
	else {
		temp2->next = temp1->next;
	}
	free(temp1);
	return 0;
}


void add_to_file_info(struct file_info *head, char name[],mode_t mode, int file_dir){

	printf("%d In add_to_file_info\n", line++);

	struct file_info *new = malloc(sizeof(struct file_info));
	struct file_info *temp;

	strcpy(new->parent, getparent(FILE_DATA, name));
	strcpy(new->name, name);
	printf("%d In add_to_file_info %s\n",line++, head->name);
    new->next 		= NULL;
    new->type 		= file_dir;
    new->file_size 	= 0;
    new->mode 		= mode;

	temp = head;
	while(temp->next != NULL) temp = temp->next;
	
	temp->next = new;
	
}


int child_exists(struct file_info *head, const char *path){

	printf("%d In child_exists\n", line++);
	head = head->next;
	
	while(head != NULL && strcmp(path,head->parent))
		head = head->next;

	if(head == NULL) return 0; 

	printf("retruning from child_exists\n");
	return 1;
}

int file_exists(const char  *path){
	printf("%d In file_exists\n", line++);
	struct file_info *head = FILE_DATA;
	struct file_info *temp;
	temp = head;

	while(temp != NULL && strcmp(temp->name, path)) temp = temp->next;

	if(temp == NULL) return 0;

	else return 1;
}



static int ramdisk_getattr(const char *path, struct stat *stbuf){

	printf("%d In getattr\n", line++);
	
	int res = 0;
	struct file_info *head = FILE_DATA;
	struct file_info *temp;

	memset(stbuf, 0, sizeof(struct stat));

	printf("%d %s\n", line++,path);
	temp = head;
	while(temp != NULL && strcmp(temp->name, path)) {
		printf("%d Inside loop %s\n", line++, temp->name);
		temp = temp->next;
	}
	printf("%d Out of loop\n", line++);

	/*if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else */if (temp == NULL) res = -ENOENT;
	
	else if(temp->type == 0){

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else {
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = temp->file_size;
	}

	return res;
}


int ramdisk_mkdir(const char *path, mode_t mode){

	printf("%d In mkdir\n", line++);
	if(file_exists(path)) perror(EEXIST);
	add_to_file_info(FILE_DATA, path, mode, 0);
	printf("%d Parent: %s\n", line++, getparent(FILE_DATA, path));
	return 0;

    
}


int ramdisk_rmdir(const char *path)
{
	printf("%d In rmdir\n", line++);

    struct file_info *head = FILE_DATA;

    if(child_exists(head, path)) return EEXIST;

    return remove_from_file_info(head, path);

}


int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{

	struct dirent *dir = malloc(sizeof(struct dirent *));

	strcpy(dir->d_name, path);
	printf("%d In readdir\n", line++);

    struct file_info *head = FILE_DATA;

    while(head != NULL){
		if(!strcmp(path, head->parent))
			if (filler(buf, head->name, NULL, 0) != 0)
	    		return -ENOMEM;
	    head = head->next;
    }

	return dir;

}


struct fuse_operations ramdisk_operations = {
	 .getattr 	= ramdisk_getattr,
	/*.open		= ramdisk_open,	
	//.flush 		= ramdisk_flush,
	.read		= ramdisk_read,	
	.write 		= ramdisk_write,
	.mknod 		= ramdisk_mknod,*/
	.mkdir 		= ramdisk_mkdir,
	/*.unlink 	= ramdisk_unlink,*/
	.rmdir 		= ramdisk_rmdir,
	/*.opendir 	= ramdisk_opendir,*/
  	.readdir 	= ramdisk_readdir,
};



int main(int argc, char *argv[]){
	struct file_info *files = NULL;
	int ret;
	umask(0);

	files = malloc(sizeof(struct file_info));

	strcpy(files->name, "/");
	files->next 		= NULL;
    files->type 		= 0;
    files->file_size 	= 512;
    files->mode 		= S_IFDIR | 0755;

	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr, "Connot run RAMDISK as root\n");
		return 1;
    }

    if (argc < 2){
    	fprintf(stderr, "Usage: %s <mountPoint> <filesysmtesize>\n", argv[0] );
    }
    else {
    	argv[argc - 1] = NULL;
    	argc--;
    }


    ret = fuse_main(argc, argv, &ramdisk_operations, files);

    return ret;
}