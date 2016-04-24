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
	char *name;
	char *parent;
	int type; // 0 = directory 1 = regualr file
	struct file_info *next;
	int file_size;
	mode_t mode;
	dev_t dev;
	char *contents;
	time_t actual_time;
	//struct stat st;
};

int line=0;


char * getparent(struct file_info *head, char *name){
	int i=0; char *temp = malloc(sizeof(char)*strlen(name));

	for(i =strlen(name)-1; i >= 0 && name[i] != '/'; i--) ;
	if(i == 0) i++;
	strncpy(temp, name, i);

	return temp;
}


int remove_from_file_info(struct file_info *head, char name[], int type){

	printf("%d remove_from_file_info \n", line++);
	struct file_info *temp1, *temp2;
	temp1 = temp2 = head;
	
	while(temp1 != NULL && strcmp(temp1->name, name)){
		temp2 = temp1;
		temp1 = temp1->next;
	}

	if(temp1 == NULL) return ENOENT;
	
	else if(temp1->type != type) return ENOTDIR;
	
	else temp2->next = temp1->next;

	head->file_size += temp1->file_size;

	free(temp1);
	return 0;
}


int add_to_file_info(struct file_info *head, char name[],mode_t mode, int file_dir, dev_t dev){

	printf("%d In add_to_file_info\n", line++);

	struct file_info *new = malloc(sizeof(struct file_info));
	struct file_info *temp;

	new->name 	= malloc(sizeof(char)*strlen(name));
	new->parent = malloc(sizeof(char)*strlen(getparent(FILE_DATA,name)));
	strcpy(new->parent, getparent(FILE_DATA, name));
	fprintf(stdout, "%d Parent: %s\n",line++, new->parent);
	fflush(stdout);
	strcpy(new->name, name);
    new->next 		= NULL;
    new->type 		= file_dir;
    new->file_size 	= 0;
    new->mode 		= mode;
    new->contents	= NULL;
   	if(file_dir == 1) new->dev = dev;
   	else new->dev 	= NULL;


	temp = head;
	while(temp->next != NULL) temp = temp->next;
	
	temp->next = new;
	
	return 0;
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

struct file_info * get_file(const char *path){
	struct file_info *head = FILE_DATA;
	printf("In get_file\n");
	while(head != NULL && strcmp(head->name, path)) head = head->next;
	return head;
}

static int ramdisk_getattr(const char *path, struct stat *stbuf){

	fprintf(stdout, "%d In getattr\n", line++);
	fflush(stdout);
	
	int res = 0;
	struct file_info *head = FILE_DATA;
	struct file_info *temp;

	memset(stbuf, 0, sizeof(struct stat));

	printf("%d %s\n", line++,path);
	temp = head;
	while(temp != NULL && strcmp(temp->name, path)) temp = temp->next;

	printf("%d Out of loop\n", line++);

	if (temp == NULL) res = -ENOENT;
	
	else if((temp->type == 0) || !strcmp(temp->name, "/")){

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	} 
	else{
		res = 0;
		stbuf->st_nlink = 1;
		//stbuf->st_dev 	= temp->dev;
		stbuf->st_size 	= temp->file_size;
		switch(temp->mode & S_IFMT){
			case S_IFIFO: 	stbuf->st_mode 	= S_IFIFO | 0644;
							break;
			case S_IFCHR: 	stbuf->st_mode 	= S_IFCHR | 0644;
							break;
			case S_IFBLK: 	stbuf->st_mode 	= S_IFBLK | 0644;
							break;
			case S_IFREG: 	stbuf->st_mode 	= S_IFREG | 0644;
							break;
			case S_IFSOCK: 	stbuf->st_mode 	= S_IFSOCK | 0644;
							break;
			case S_IFLNK: 	stbuf->st_mode 	= S_IFLNK | 0644;
							break;
			default: res = -ENOENT;
		}
	}
	return res;
}


int ramdisk_mkdir(const char *path, mode_t mode){

	if(file_exists(path)) perror(EEXIST);
	add_to_file_info(FILE_DATA, path, mode, 0, NULL);
	return 0;
   
}


int ramdisk_rmdir(const char *path){
	printf("%d In rmdir\n", line++);

    struct file_info *head = FILE_DATA;

    if(!file_exists(path)) return ENOENT;

    if(child_exists(head, path)) return EEXIST;

    return remove_from_file_info(head, path,0);

}



int ramdisk_unlink(const char *path){

	printf("%d In rmdir\n", line++);

    struct file_info *head = FILE_DATA;

    if(!file_exists(path)) return ENOENT;

    return remove_from_file_info(head, path, 1);
}



char * strip_parent(struct file_info *temp){
	int i,j;
	char *string = malloc(sizeof(char)*(strlen(temp->name) - strlen(temp->parent) +1));

	for (i = strlen(temp->parent), j = 0; i < strlen(temp->name); i++, j++) string[j] = temp->name[i];

	string[j] = '\0';
	return string;
}


int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

	(void) offset;
	(void) fi;
	
	printf("%d In readdir\n", line++);

    struct file_info *head = FILE_DATA;


    while(head != NULL){
    	printf("Inside loop path:%s parent: %s current:%s\n",path, head->parent, head->name);
		if(head->parent != NULL && !strcmp(path, head->parent)){
			if(!strcmp(head->parent, "/")){
				if (filler(buf, head->name + strlen(head->parent), NULL, 0) != 0){
					printf("Error Writing into buffer\n");
	    			return -ENOMEM;
				}
			}
			else if(filler(buf, head->name + strlen(head->parent) + 1, NULL, 0) != 0){
				printf("Error Writing into buffer\n");
	    		return -ENOMEM;
			}
	    	printf("Writing into buffer\n");
		}
	    head = head->next;
    }

	filler(buf, "..", NULL, 0);
	filler(buf, ".", NULL, 0);

	return 0;

}

int ramdisk_mknod(const char *path, mode_t mode, dev_t dev){
	printf("%d Inside mknod\n",line++);
    int retstat;
    char *parent_path;

    if(file_exists(path)) return(EEXIST);

    else if((parent_path = getparent(FILE_DATA, path)) == NULL) return(ENOENT);

	retstat = add_to_file_info(FILE_DATA, path, mode, 1, dev);

	return 0;
}


int ramdisk_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

	(void) offset;
	(void) fi;


	fprintf(stdout, "%d In ramdisk_read\n", line++);

    struct file_info *head = FILE_DATA;
    if(!file_exists(path)) return EBADF;

    struct file_info * temp;
    temp = get_file(path);
    if(temp != NULL && temp->type == 0) return EISDIR;

    if(temp->file_size == 0){
    	strcpy(buf, "\0");
    	return 0;
    }
    strcpy(buf, temp->contents);
    return temp->file_size;
}


int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    (void) offset;
	(void) fi;
	(void) size;

	fprintf(stdout, "%d In ramdisk_write\n", line++);
	fflush(stdout);
    struct file_info *head = FILE_DATA;
    if(!file_exists(path)) return EBADF;

    struct file_info * temp;
    temp = get_file(path);

    if(temp != NULL && temp->type == 0) return EISDIR;

    if((head->file_size - strlen(buf)) < 0) return ENOSPC;

    if(temp->file_size == 0){

		fprintf(stdout, "%d Inside write\n", line++);
		fflush(stdout);
		printf("Calling malloc\n");
    	temp->contents = malloc(sizeof(char)*strlen(buf));
    	strcpy(temp->contents, buf);
    	temp->file_size = strlen(buf);
    	head->file_size -= temp->file_size;
    	return strlen(buf);
    }
    else{
    	fprintf(stdout, "%d Inside write\n", line++);
    	temp->contents = (char *) realloc(temp->contents, temp->file_size + strlen(buf));
    	strcat(temp->contents, buf);
    	temp->file_size = strlen(temp->contents);
    	head->file_size -= strlen(buf);
    	return 0;
    }
}


int ramdisk_open(const char *path, struct fuse_file_info *fi){
    int retstat = 0;
	(void) fi;
	printf("%d Inside ramdisk_open\n",line++);
	if(!file_exists(path)) return EBADF;
    
    return retstat;
}


int ramdisk_flush(const char *path, struct fuse_file_info *fi)
{
    printf("%d ramdisk_flush\n",line++);
    (void) fi;

		
    return 0;
}


int ramdisk_truncate(const char *path, off_t newsize){
    printf("%d ramdisk_truncate newsize: %d\n",line++, newsize);


	if(!file_exists(path)) return EBADF;

	struct file_info * temp;
    temp = get_file(path);

    if(newsize == 0){
    	free(temp->contents);
    	temp->file_size = 0;
    	return 0;
    }
    temp->contents = (char *) realloc(temp->contents, (int)newsize);
    if(temp->file_size > (int)newsize) temp->contents[(int)newsize] = '\0';

    temp->file_size = (int)newsize;
    return 0;
}

int ramdisk_release(const char *path, struct fuse_file_info *fi){
    (void)fi;
    printf("%d ramdisk_release\n",line++);

    return 0;
}


int ramdisk_utime(const char *path, struct utimbuf *ubuf){
    
    printf("%d ramdisk_utime\n",line++);

   	return 0;
}

struct fuse_operations ramdisk_operations = {
	 .getattr 	= ramdisk_getattr,
	.open		= ramdisk_open,	
	.flush 		= ramdisk_flush,
	.read		= ramdisk_read,	
	.write 		= ramdisk_write,
	.mknod 		= ramdisk_mknod,
	.mkdir 		= ramdisk_mkdir,
	.unlink 	= ramdisk_unlink,
	.rmdir 		= ramdisk_rmdir,
	/*.opendir 	= ramdisk_opendir,*/
  	.readdir 	= ramdisk_readdir,
  	.truncate 	= ramdisk_truncate,
  	.release 	= ramdisk_release
};



int main(int argc, char *argv[]){
	struct file_info *files = NULL;
	int ret;
	umask(0);

	files = malloc(sizeof(struct file_info));
	files->name 		= malloc(sizeof(char)*3);
	strcpy(files->name, "/");
	files->parent		= NULL;
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
