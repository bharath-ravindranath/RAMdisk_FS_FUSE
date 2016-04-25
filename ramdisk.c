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

enum fileType {T_DIR, T_REG};

typedef enum fileType fileType;

struct file_info {
	char *name;
	struct file_info *parent;
	fileType type; // 0 = directory 1 = regualr file
	struct file_info *next;
	char *contents;
	struct stat st;
};

//int line=0;

struct file_info * get_file(const char *path){
	struct file_info *head = FILE_DATA;
	//printf("In get_file %s\n",path);
	while(head != NULL && strcmp(head->name, path)) head = head->next;
	//printf("File %s\n", head->name);
	return head;
}

struct file_info * getparent(struct file_info *head, const char *name){
	int i = strlen(name)-1, j; 
	char *temp;
	//printf("In get parent got path: %s len %d\n",name, strlen(name));
	for(; i >= 0 && name[i] != '/'; i--) ;
	if(i == 0) i++;
	//printf("After parse %d\n", i);
	temp = malloc(sizeof(char)*(i+1));
	for(j=0; j < i; j++) temp[j] = name[j];
	temp[j] = '\0';
	//printf("In get Parent: parent string: %s\n",temp);
	return get_file(temp);
}


int remove_from_file_info(struct file_info *head, const char *name, fileType type){

	//printf("%d remove_from_file_info \n", line++);
	struct file_info *temp1, *temp2;
	temp1 = temp2 = head;
	
	while(temp1 != NULL && strcmp(temp1->name, name)){
		temp2 = temp1;
		temp1 = temp1->next;
	}

	if(temp1 == NULL) return ENOENT;
	
	else if(temp1->type != type) return ENOTDIR;
	
	else temp2->next = temp1->next;

	head->st.st_size += temp1->st.st_size;

	free(temp1);
	return 0;
}


int add_to_file_info(struct file_info *head, const char *name,mode_t mode, fileType type){

	//printf("%d In add_to_file_info\n", line++);

	struct file_info *new = malloc(sizeof(struct file_info));
	struct file_info *temp;

	new->name 	= malloc(sizeof(char)*strlen(name));
	new->parent = getparent(FILE_DATA, name);
	//fprintf(stdout, "%d Parent: %s\n",line++, new->parent->name);
	fflush(stdout);
	strcpy(new->name, name);
    new->next 			= NULL;
    new->type 			= type;
    new->st.st_size 	= 0;
    new->contents		= NULL;
    new->st.st_mode 	= mode;
    new->st.st_atime 	= time(0);
    new->st.st_ctime 	= time(0);
    new->st.st_mtime 	= time(0);
   	if(type == T_REG) new->st.st_nlink 	= 1;

   	else new->st.st_nlink	= 2;

	temp = head;
	while(temp->next != NULL) temp = temp->next;
	
	temp->next = new;
	
	

	return 0;
}


int child_exists(struct file_info *head, const char *path){

	//printf("%d In child_exists\n", line++);
	head = head->next;
	
	while(head != NULL && strcmp(path,(head->parent)->name))
		head = head->next;

	if(head == NULL) return 0; 

	//printf("retruning from child_exists\n");
	return 1;
}

int file_exists(const char  *path){
	//printf("%d In file_exists\n", line++);
	struct file_info *head = FILE_DATA;
	struct file_info *temp;
	temp = head;

	while(temp != NULL && strcmp(temp->name, path)) temp = temp->next;

	if(temp == NULL) return 0;

	else return 1;
}



static int ramdisk_getattr(const char *path, struct stat *stbuf){

	//fprintf(stdout, "%d In getattr\n", line++);
	fflush(stdout);
	
	int res = 0;
	struct file_info *head = FILE_DATA;
	struct file_info *temp;

	memset(stbuf, 0, sizeof(struct stat));

	temp = head;
	while(temp != NULL && strcmp(temp->name, path)) temp = temp->next;


	if (temp == NULL) res = -ENOENT;
	
	else {
		res = 0;
		//stbuf->st_nlink 	= temp->st.st_nlink;
		stbuf->st_atime 	= temp->st.st_atime;
    	stbuf->st_ctime 	= temp->st.st_ctime;
    	stbuf->st_mtime 	= temp->st.st_mtime;
		
		if(temp->type == T_DIR)	{
			stbuf->st_mode 		= S_IFDIR | 0755;
			stbuf->st_nlink		= temp->st.st_nlink;
		}
		else{

			stbuf->st_nlink	= 1;
			stbuf->st_size 	= temp->st.st_size;
			switch(temp->st.st_mode & S_IFMT){
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
	}
	return res;
}


int ramdisk_mkdir(const char *path, mode_t mode){

	if(file_exists(path)) return(EEXIST);
	return add_to_file_info(FILE_DATA, path, mode, T_DIR);
   
}


int ramdisk_rmdir(const char *path){
	//printf("%d In rmdir\n", line++);

    struct file_info *head = FILE_DATA;

    if(!file_exists(path)) return ENOENT;

    if(child_exists(head, path)) return -1;

    return remove_from_file_info(head, path, T_DIR);

}



int ramdisk_unlink(const char *path){

	//printf("%d In rmdir\n", line++);

    struct file_info *head = FILE_DATA;

    if(!file_exists(path)) return ENOENT;

    return remove_from_file_info(head, path, T_REG);
}


int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){

	(void) offset;
	(void) fi;
	
	//printf("%d In readdir\n", line++);

    struct file_info *head = FILE_DATA;

	filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    while(head != NULL){
		if((head->parent != NULL) && (!strcmp(path, (head->parent)->name))){
			if (filler(buf, head->name + strlen((head->parent)->name) + (!strcmp((head->parent)->name, "/")?0:1), NULL, 0) != 0){
				//printf("Error Writing into buffer\n");
    			return -ENOMEM;
			}
    	//printf("Writing into buffer\n");
		}
	    head = head->next;
    }
	return 0;
}

int ramdisk_mknod(const char *path, mode_t mode, dev_t dev){
	//printf("%d Inside mknod\n",line++);
    struct file_info *parent_path = getparent(FILE_DATA, path);

    if(file_exists(path)) return(EEXIST);

   //printf("Mknod parent: %s\n", parent_path->name);
    if(parent_path == NULL) return(ENOENT);

	return add_to_file_info(FILE_DATA, path, mode, T_REG);
}


int ramdisk_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

    if(!file_exists(path)) return EBADF;

    struct file_info * temp;
    temp = get_file(path);
    if(temp != NULL && temp->type == T_DIR) return EISDIR;

    if(temp->st.st_size == 0 || size == 0){
    	//printf("In ramdisk_read inside zero\n");
    	strcpy(buf, "\0");
    	return 0;
    }
    if(offset == 0 && size >= temp->st.st_size){
    	//fprintf("In ramdisk_read inside full read\n");

    	strcpy(buf, temp->contents);
    	return temp->st.st_size;
    }
    else{
    	strncpy(buf, &(temp->contents[offset]), size);
    	return strlen(buf);
    }
}


int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
   
	//fprintf(stdout, "%d In ramdisk_write\n", line++);
	fflush(stdout);
    struct file_info *head = FILE_DATA;
    if(!file_exists(path)) return EBADF;

    struct file_info * temp;
    temp = get_file(path);

    if(temp != NULL && temp->type == T_DIR) return EISDIR;

    if((head->st.st_size - strlen(buf)) < 0) return ENOSPC;

    if(temp->st.st_size == 0){

		//fprintf(stdout, "%d Inside write\n", line++);
		fflush(stdout);
		//printf("Calling malloc\n");
    	temp->contents = malloc(sizeof(char)*strlen(buf));
    	strcpy(temp->contents, buf);
    	temp->st.st_size = strlen(buf);
    	head->st.st_size -= temp->st.st_size;
    	return strlen(buf);
    }
    else{

    	//fprintf(stdout, "%d Inside write concatenate\n", line++);
    	temp->contents = (char *) realloc(temp->contents, temp->st.st_size + strlen(buf));
    	strncat(&(temp->contents[offset]), buf, size);
    	temp->st.st_size = strlen(temp->contents);
    	head->st.st_size -= strlen(buf);
    	return strlen(buf);
    }
}


int ramdisk_open(const char *path, struct fuse_file_info *fi){

	(void) fi;
	//printf("%d Inside ramdisk_open\n",line++);

	if(!file_exists(path)) return ENOENT;

	struct file_info *temp = get_file(path);

	if(temp->type == T_DIR) return EISDIR;

	return 0;
}


int ramdisk_opendir(const char *path, struct fuse_file_info *fi){
    (void)fi;
   //printf("%d ramdisk_opendir\n",line++);
    
    if(!file_exists(path)) return ENOENT;

    struct file_info *temp = get_file(path);

	if(temp->type != T_DIR) return ENOTDIR;

	return 0;

}

int ramdisk_flush(const char *path, struct fuse_file_info *fi){
   //printf("%d ramdisk_flush\n",line++);
    (void) fi;

    if(!file_exists(path)) return ENOENT;

	struct file_info *temp = get_file(path);

	if(temp->type == T_DIR) return EISDIR;

	temp->st.st_nlink--;
		
    return 0;
}


int ramdisk_truncate(const char *path, off_t newsize){
   //printf("%d ramdisk_truncate newsize: %d\n",line++, newsize);
    struct file_info *head = FILE_DATA;

	if(!file_exists(path)) return EBADF;

	struct file_info * temp;
    temp = get_file(path);

    if(newsize == 0){
    	//printf("%d ramdisk_truncate inside newsize 0\n",line++, newsize);

    	free(temp->contents);
    	head->st.st_size -= temp->st.st_size;
    	temp->st.st_size = 0;
    	return 0;
    }
    temp->contents = (char *) realloc(temp->contents, (int)newsize);
    if(temp->st.st_size > (int)newsize) temp->contents[(int)newsize] = '\0';
    head->st.st_size -= temp->st.st_size;

    temp->st.st_size = (int)newsize;
    head->st.st_size += temp->st.st_size;

    return 0;
}

int ramdisk_release(const char *path, struct fuse_file_info *fi){
    (void) fi;
	//printf("%d Inside ramdisk_open\n",line++);

	if(!file_exists(path)) return ENOENT;

	struct file_info *temp = get_file(path);

	if(temp->type == T_DIR) return EISDIR;

	return 0;
}


int ramdisk_rename(const char *path, const char *newpath)
{

	//fprintf(stdout, "old: %s New: %s\n",path, newpath );

	if(!file_exists(path)) return ENOENT;

	struct file_info *temp = get_file(path);

	if(temp->type == T_REG){
		if(file_exists(newpath)){
			struct file_info *new = get_file(newpath);
			if(new->type == T_DIR) return EISDIR;
			//printf("In both file exists");
			free(temp->name);
			temp->name = malloc(sizeof(char)*strlen(newpath));
			strcpy(temp->name,newpath);
			temp->parent = new->parent;
			remove_from_file_info(FILE_DATA, newpath, T_REG);
		}

		else{
			struct file_info *parent = getparent(FILE_DATA,newpath);

			if(parent == NULL) return ENOENT;
			if(parent->type == T_REG) return ENOTDIR;
			free(temp->name);
			temp->name = malloc(sizeof(char)*strlen(newpath));
			strcpy(temp->name,newpath);
			temp->parent = parent;
		}
	}

	else{
		if(file_exists(newpath)){
			if((strlen(newpath) > strlen(path)) && !strncmp(path,newpath,strlen(path))){
				return ELOOP;
			}

			struct file_info *new = get_file(newpath);
			if(new->type == T_REG) return ENOTDIR;
			
			struct file_info *head = FILE_DATA;
			while(head != NULL){
				if(head->parent != NULL && !strcmp((head->parent)->name,newpath))
					return ENOTEMPTY;
			}
			temp->name = realloc(temp->name, sizeof(char)*strlen(newpath));
			strcpy(temp->name, newpath);
			temp->parent = new->parent;
			remove_from_file_info(FILE_DATA, newpath, T_DIR);
		}		
		else{
			struct file_info *parent = getparent(FILE_DATA,newpath);
			if(parent == NULL) return ENOENT;
			if(parent->type != T_DIR) return ENOTDIR;
			temp->name = realloc(temp->name, sizeof(char)*strlen(newpath));
			strcpy(temp->name,newpath);
			temp->parent = parent;

			struct file_info *head = FILE_DATA;
			while(head != NULL){
				if((!strncmp(path, head->name, strlen(path))) && (head->name[strlen(path)] == '/')){
					char *new_name = malloc(sizeof(char)*(strlen(temp->name)+strlen(head->name)- strlen(path)));
					strcpy(new_name,newpath);
					strcat(new_name,&(head->name[strlen(path)]));
					head->name = realloc(head->name,sizeof(char)*strlen(new_name));
					strcpy(head->name,new_name);
				}
				head = head->next;
			}
		}
	}
	return 0;
}


int ramdisk_utime(const char *path, struct utimbuf *ubuf){
    
   //printf("%d ramdisk_utime\n",line++);

	if(!file_exists(path)) return ENOENT;

	struct file_info *temp = get_file(path);

	temp->st.st_atime = ubuf->actime;
	temp->st.st_mtime = ubuf->modtime;

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
	.opendir 	= ramdisk_opendir,
  	.readdir 	= ramdisk_readdir,
  	.truncate 	= ramdisk_truncate,
  	.release 	= ramdisk_release,
  	.utime 		= ramdisk_utime,
  	.rename 	= ramdisk_rename
};



int main(int argc, char *argv[]){
	struct file_info *files = malloc(sizeof(struct file_info));;
	int ret;
	umask(0);


	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr, "Connot run RAMDISK as root\n");
		return 1;
    }


    if (argc < 2){
    	fprintf(stderr, "Usage: %s <filesysmtesize_mount_point>\n", argv[0] );
    }
    else {
    	files->st.st_size 	= atoi(argv[argc-1]);    	
    	argv[argc - 1] = NULL;
    	argc--;
    }

	
	files->name 		= malloc(sizeof(char)*3);
	strcpy(files->name, "/");
	files->parent		= NULL;
	files->next 		= NULL;
    files->type 		= T_DIR;
    files->st.st_atime 	= time(0);
    files->st.st_ctime 	= time(0);
    files->st.st_mtime 	= time(0);

    ret = fuse_main(argc, argv, &ramdisk_operations, files);

    return ret;
}