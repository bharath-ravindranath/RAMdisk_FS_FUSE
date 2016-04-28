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


enum fileType{T_DIR, T_REG, T_ROOT} typedef fileType;


struct file{
	char *name;
	char *contents;
	fileType type;
	size_t size;
	struct stat st;
	struct file *parent;
	struct file *next;
	int children;
} typedef file;

file *root;
int line = 0;

file * getFile(const char *path){
	//printf("Inside getFile\n");
	file *temp = root;
	while(temp != NULL && strcmp(temp->name, path)){
	
		temp = temp->next;
	}
	return temp;
}


file * getParent(const char *path){
	file *temp = root;
	int i=0; 
	
	//printf("In getParent\n");

	for(i =strlen(path) - 1; i >= 0 && path[i] != '/'; i--) ;
	if(i == 0) i++;
	
	while(temp != NULL && strncmp(temp->name, path, i)) 
		temp = temp->next;
	return temp;
}


int addFile(const char *path, mode_t mode, fileType type, file *parent){
	//printf("inside addFile\n");
	file *temp 	= (file *)malloc(sizeof(file));
	file *temp1 = root;
	if(temp == NULL){
		perror("Malloc error\n");
		exit(0);
	}

	temp->name = (char *)malloc(sizeof(char)*strlen(path)+1);
	if(temp->name == NULL){
		perror("Malloc error\n");
		return 0;
	}
	strcpy(temp->name, path);
	temp->contents		= NULL;
	temp->type 			= type;
	temp->next 			= NULL;
	temp->st.st_mode	= mode;
	temp->size 			= 0;
	if((temp->parent = getParent(path)) == NULL) return -ENOTDIR;
	temp->parent->children ++;
	while(temp1->next != NULL) 	temp1 = temp1->next;

	temp1->next = temp;
	return 0;

}

int removeFile(const char *path, fileType type, file *fileToRemove){
	//printf("inside removeFile\n");
	fileToRemove->parent->children--;

	file *temp = root;
	while(temp->next != NULL && strcmp(temp->next->name, fileToRemove->name))	temp = temp->next;

	if(temp == NULL) return -1;

	temp->next = fileToRemove->next;

	if(type == T_REG){
		root->size += fileToRemove->size;
		if(fileToRemove->contents != NULL){
			free(fileToRemove->contents);
			fileToRemove->contents = NULL;
		}
	}
	free(fileToRemove->name);
	fileToRemove->name = NULL;
	free(fileToRemove);
	return 0;
}

static int ramdisk_getattr(const char *path, struct stat *stbuf){

	//printf("In ramdisk_getattr\n");

	file *temp = getFile(path);;

	memset(stbuf, 0, sizeof(struct stat));

	if (temp == NULL) return -ENOENT;
	
	else if(temp->type == T_DIR || temp->type == T_ROOT){

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} 
	else{
		stbuf->st_nlink = 1;
		stbuf->st_size 	= temp->size;
		stbuf->st_mode 	= S_IFREG | 0644;
		/*switch(temp->st.st_mode & S_IFMT){
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
			default: return -ENOENT;
		}*/
		return 0;
	}
}



int ramdisk_mkdir(const char *path, mode_t mode){

	//printf("%d In mkdir\n", line++);
	file *temp = getFile(path);

	if(temp != NULL) return(EEXIST);
	
	return	addFile(path, mode, T_DIR, getParent(path));
   
}


int ramdisk_rmdir(const char *path){

	//printf("%d In rmdir\n", line++);

	file *temp;
    if((temp = getFile(path)) == NULL) return -ENOENT;

    if(temp->children > 0) return -ENOTEMPTY;

    if(temp->type == T_REG) return -ENOTDIR;

    if(temp->type == T_ROOT) return -EBUSY;

    return removeFile(path,T_DIR, temp);

}

int ramdisk_opendir(const char *path, struct fuse_file_info *fi){
    (void)fi;
	//printf("%d ramdisk_opendir\n",line++);
    file *temp;
    if((temp = getFile(path)) == NULL) return -ENOENT;

	if(temp->type != T_DIR && temp->type != T_ROOT) return -ENOTDIR;

	return 0;
}


int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

	(void) offset;
	(void) fi;
	
	//printf("%d In readdir\n", line++);

	file *temp = getFile(path);

	if(temp == NULL) return -ENOENT;

    while(temp != NULL){
		if(temp->parent != NULL && !strcmp(path, temp->parent->name)){
			if(!strcmp(temp->parent->name, "/")){
				if (filler(buf, temp->name + strlen(temp->parent->name), NULL, 0) != 0){
					////printf("Error Writing into buffer\n");
	    			return -ENOMEM;
				}
			}
			else if(filler(buf, temp->name + strlen(temp->parent->name) + 1, NULL, 0) != 0){
				////printf("Error Writing into buffer\n");
	    		return -ENOMEM;
			}
	    	////printf("Writing into buffer\n");
		}
	    temp = temp->next;
    }

	filler(buf, "..", NULL, 0);
	filler(buf, ".", NULL, 0);

	return 0;

}


int ramdisk_mknod(const char *path, mode_t mode, dev_t dev){

	//printf("%d Inside mknod\n",line++);

	file *temp;

	if((temp = getFile(path)) != NULL) return -EEXIST;

	return addFile(path, mode, T_REG, getParent(path));
}


int ramdisk_open(const char *path, struct fuse_file_info *fi){
    (void)fi;
	//printf("%d ramdisk_open\n",line++);
    file *temp;
    if((temp = getFile(path)) == NULL) return -ENOENT;

	if(temp->type != T_REG) return -EISDIR;

	return 0;
}


int ramdisk_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

	//(void) offset;
	(void) fi;


	//printf("%d In ramdisk_read\n", line++);

	file *readFile = getFile(path);

    if(readFile == NULL) return EBADF;

    if(readFile->type != T_REG) return EISDIR;

    if(readFile->size == 0){
    	strcpy(buf, "\0");
    	return 0;
    }

	memset(buf, '\0', size+1);
    strncpy(buf, &(readFile->contents[offset]), size);
    return size;
}



int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

	(void) fi;

	file *writeFile = getFile(path);
	//printf("Inside Write 1\n");

	if(writeFile == NULL) return -ENOENT;


	if(writeFile->type != T_REG) return -EISDIR;

	if((root->size - size) < 0) return -ENOSPC;

	if(size == 0) return 0;

	if(writeFile->size == 0){
		//printf("Inside Write 2\n");

		writeFile->contents = (char *) malloc(sizeof(char)*(size+1));
		if(writeFile->contents == NULL){
			perror("Malloc error\n");
			return -1;
		}

		memset(writeFile->contents, '\0', size+1);
		memcpy(writeFile->contents, buf, size);
		writeFile->size = size;
		root->size -= size;
		//printf("%s\n", writeFile->contents);
		return size;

	}
	else{
		//printf("Inside Write 3\n");

		int strLen = strlen(writeFile->contents);
		char *temp =(char *) malloc(sizeof(char)*(size + strLen +1));
		if(temp == NULL){
			perror("Malloc error\n");
			return -1;
		}
		memset(temp, '\0', size + strLen +1);
		writeFile->contents = (char *)realloc((void *)writeFile->contents,sizeof(char)*(strLen+ size +1));
		strncpy(temp, writeFile->contents, offset);
		strncpy(&temp[offset], buf, size);
		strncpy(&temp[offset+size], &(writeFile->contents[offset]), strLen - offset);
		//printf("%s\n", temp);

		strcpy(writeFile->contents, temp);
		//printf("buf: %s data: %s\n", buf,writeFile->contents);
		writeFile->size += size;
		root->size -= size;
		free(temp);
		return size;
	}
}

int ramdisk_unlink(const char *path){

	//printf("%d In unlink\n", line++);

	file *temp;
    if((temp = getFile(path)) == NULL) return -ENOENT;

    if(temp->type == T_DIR || temp->type == T_ROOT) return -EISDIR;

    return removeFile(path,T_REG, temp);
}




int ramdisk_flush(const char *path, struct fuse_file_info *fi){
	//printf("%d Inside ramdisk_flush\n",line++);
    (void) fi;

    file *temp = getFile(path);
	if(temp == NULL) return -EBADF;

	if(temp->type != T_REG) return -EISDIR;

    return 0;
}


int ramdisk_truncate(const char *path, off_t newsize){
    //printf("%d ramdisk_truncate newsize: %d\n",line++, (int)newsize);

    file *temp = getFile(path);
	if(temp == NULL) return -EBADF;

	if(temp->type != T_REG) return -EISDIR;

	if(temp->contents != NULL){
		free(temp->contents);
		temp->contents = NULL;
	}
	root->size += temp->size;
	temp->size = 0;
	return 0;
}


int ramdisk_release(const char *path, struct fuse_file_info *fi)
{
	//printf("%d Inside ramdisk_release\n",line++);


	(void) fi;
	file *temp;

    if((temp = getFile(path) )== NULL){
    	//printf("I am here\n");
    	return -ENOENT;	
    }


    return 0;
}

int ramdisk_rename(const char *path, const char *newpath)
{

	//fprintf(stdout, "old: %s New: %s\n",path, newpath );


	file *temp = getFile(path);
	if(temp == NULL) return -ENOENT;

	file *new = getFile(newpath);
	if(temp->type == T_REG){
		if(new != NULL){
			if(new->type == T_DIR) return EISDIR;
			//printf("In both file exists");
			free(temp->name);
			temp->name = NULL;
			temp->name = (char *)malloc(sizeof(char)*(strlen(newpath)+1));
			strcpy(temp->name,newpath);
			temp->parent = new->parent;
			removeFile(newpath, T_REG, new);
		}

		else{
			file *parent = getParent(newpath);

			if(parent == NULL) return -ENOENT;
			if(parent->type == T_REG) return -ENOTDIR;
			free(temp->name);
			temp->name = malloc(sizeof(char)*(strlen(newpath)+1));
			strcpy(temp->name,newpath);
			temp->parent = parent;
		}
	}

	else{
		if(new != NULL){
			if((strlen(newpath) > strlen(path)) && !strncmp(path,newpath,strlen(path))){
				return -ELOOP;
			}

			if(new->type == T_REG) return ENOTDIR;
			
			if(new->children > 0) return -ENOTEMPTY;
			
			temp->name = realloc(temp->name, sizeof(char)*(strlen(newpath)+1));
			strcpy(temp->name, newpath);
			temp->parent = new->parent;
			removeFile(newpath, T_DIR, new);
		}		
		else{
			file *parent = getParent(newpath);
			if(parent == NULL) return -ENOENT;
			if(parent->type != T_DIR && parent->type != T_ROOT) return -ENOTDIR;
			temp->name = realloc(temp->name, sizeof(char)*(strlen(newpath)+1));
			strcpy(temp->name,newpath);
			temp->parent = parent;
		}
		file *temp1 = root->next;
		int len = strlen(path);
		while(temp1 != NULL){
			if(!strcmp(temp1->parent->name, newpath)){
				char *new_name = malloc(sizeof(char)*(strlen(temp1->name) + strlen(newpath)  - len + 1));
				strcpy(new_name, newpath);
				strcat(new_name, &(temp1->name[len]));
				temp1->name = (char *) realloc((void *)temp1->name, sizeof(char)*(strlen(new_name)+1));
				strcpy(temp1->name, new_name);
				free(new_name);
			}
			temp1 = temp1->next;
		}
	}
	return 0;
}

struct fuse_operations ramdisk_operations = {
	.getattr 	= ramdisk_getattr,
	.mkdir 		= ramdisk_mkdir,
	.rmdir 		= ramdisk_rmdir,
	.opendir 	= ramdisk_opendir,
  	.readdir 	= ramdisk_readdir,
	.mknod 		= ramdisk_mknod,
	.open		= ramdisk_open,	
	.read		= ramdisk_read,	
	.write 		= ramdisk_write,
	.unlink 	= ramdisk_unlink,
	.flush 		= ramdisk_flush,
  	.truncate 	= ramdisk_truncate,
  	.release 	= ramdisk_release,
  	.rename 	= ramdisk_rename
};



int main(int argc, char *argv[]){

	umask(0);

	root 		= (file *)malloc(sizeof(file));
	if(root == NULL){
		perror("Malloc error\n");
		return 0;
	}
	root->name		= (char *)malloc(sizeof(char)*2);
	if(root->name == NULL){
		perror("Malloc error\n");
		return 0;
	}
	strcpy(root->name, "/");
	//printf("%s\n", root->name);
	root->contents	= NULL;
	root->type 		= T_ROOT;
	root->parent	= NULL;
	root->next 		= NULL;
	root->children	= 0;

	if ((getuid() == 0) || (geteuid() == 0)) {
		//printf("Connot run RAMDISK as root\n");
		return 1;
    }

    if (argc < 2 || argc > 3){
    	//printf("Usage: %s <mountPoint> <filesysmtesize>\n", argv[0] );
    }
    else {
    	if(argc == 2) root->size = 512*1024*1024;

    	else {
    		root->size = atoi(argv[argc-1]) * 1024 *1024;
			argv[argc - 1] = NULL;
    		argc--;
    	}
    }

    fuse_main(argc, argv, &ramdisk_operations, root);

    free(root->name);
    free(root);

    return 0;
}