/***********************************************************************
name: Ben Mikailenko
readable -- recursively count readable files.
description:
See CS 360 Files and Directories lecture for details.
***********************************************************************/

/* Includes and definitions */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <libgen.h>
#include <time.h>

extern int errno;
struct stat st = {0};
pthread_t fileCopy_thread[1000], fileRestore_thread[1000];

// File node in a linked list
typedef struct Node {
    struct Node *next;
    char name[256];
    char path[256];
    int type;
	int thread_num;
} Node;

// recursivley creates directories
static void _mkdir(const char *dir) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);

        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++) {
			if(*p == '/') {
                *p = 0;
                mkdir(tmp, S_IRWXU);
                *p = '/';
           }
		}     
        mkdir(tmp, S_IRWXU);
}

// returns last modified time of a file
static time_t lastModifiedTime(const char *path) {
    struct stat attr;
    if (stat(path, &attr) == 0) {
        return attr.st_mtime;
    }
    return 0;
}

// creates a node in a linked list
Node *createNode(char *name, int type) {
	Node *temp = malloc(sizeof(Node));
	strcpy(temp->name, name);
	temp->type = type;
	temp->next = NULL;
	temp->thread_num = 0;
	return temp;
}

// frees all nodes in a linked list
void freeNodes(Node *head) {
	Node *temp = head;
	Node *prev;
	while (temp != NULL) {
		prev = temp;
		temp = temp->next;
		free(prev);
	}
	free(prev);
}

// pushes a node into a linked list
Node *push(Node *head, Node *temp) {
	temp->next = head;
	return temp;
}

// get the number of nodes in a linked list
int getNumFiles(Node *head) {
	int num_files = 0;
	Node *temp = head;
	while (temp != NULL) {
		num_files += 1;
		temp = temp->next;
	}
	return num_files;
}

// thread restore function restores a file from .backup given file node
void *restore(void *node) {
	printf("[Thread %d] Restoring %s\n", ((Node *) node)->thread_num, ((Node *) node)->name);

	FILE *in, *out;
	char c;
	int num_bytes = 0;

	// get the path excluding "./backup"
	char *new_path = ((Node *) node)->path + 8;

	// get backup file path
	char old_path[256];
	strcpy(old_path, ((Node *) node)->path);
	strcat(old_path, ((Node *) node)->name);

	// get the new folder path
	char *new_folder_path = old_path + 8;

	// get the new name without ".bak"
	char new_name[256];
	((Node *) node)->name[strlen(((Node *) node)->name) - 4] = '\0';
	strcpy(new_name, ((Node *) node)->name);

	// if file is a folder
	if (((Node *) node)->type == 4) {

		// recursivley create folder if it doesn't exist
		if (stat(new_folder_path, &st) == -1) {

			printf("[Thread %d] Restored folder %s\n", ((Node *) node)->thread_num, new_folder_path);
			_mkdir(new_folder_path);

		} else {
			return (void *) 1; // if folder exists, just return 1
		}
	}

	// if file is actually a file
	if (((Node *) node)->type == 8) {

		// recursivley create path to file if it doesn't exist
		if (stat(new_path, &st) == -1) {
			_mkdir(dirname(new_path));
		}

		// open the old file
		in = fopen(old_path, "r");

		if (in == NULL) {
			fprintf(stderr, "Value of errno: %d\n", errno);
			fprintf(stderr, "Error opening in file: %s\n", strerror( errno ));
   		}

		//
		if (stat(new_path, &st) == -1) { // if old file doesn't exist
		//

			// create a new file
			out = fopen(strcat(new_path, new_name), "w+");

			if (out == NULL) {
				fprintf(stderr, "Value of errno: %d\n", errno);
				fprintf(stderr, "Error opening out file: %s\n", strerror( errno ));
			}


			if (in != NULL && out != NULL) {

				while((c = fgetc(in)) != EOF) { // copy file file contents
					fputc(c, out);
					num_bytes++;
				}
					
				// free memory
				fclose(in);
				fclose(out);

				printf("[Thread %d] Restored %d bytes from %s to %s\n", ((Node *) node)->thread_num, num_bytes, old_path, new_path);
			
			}

		//
		} else { // if old file does exist
		//

			time_t t_1 = lastModifiedTime(old_path);
			time_t t_2 = lastModifiedTime(new_path);

			if (t_2 < t_1) { // and file is older than the one we're trying to copy

				printf("[Thread %d] WARNING: overwriting %s\n", ((Node *) node)->thread_num, new_path);

				// open the old file for overwriting
				out = fopen(strcat(new_path, new_name), "w+");

				if (out == NULL) {
					fprintf(stderr, "Value of errno: %d\n", errno);
					fprintf(stderr, "Error opening out file: %s\n", strerror( errno ));
				}

				if (in != NULL && out != NULL) {
					while((c = fgetc(in)) != EOF) { // copy file contents
						fputc(c, out);
						num_bytes++;
					}
						
					// free memory
					fclose(in);
					fclose(out);

					printf("[Thread %d] Restored %d bytes from %s to %s\n", ((Node *) node)->thread_num, num_bytes, old_path, new_path);
				}

			} else {
				return (void *) 1; // if old file is newer then just return 1
			}
		}
	}
	return (void *) 0;
}


// thread function to copy a file to ".backup"
void *copy(void *node) {
	printf("[Thread %d] Backing up %s\n", ((Node *) node)->thread_num, ((Node *) node)->name);

	FILE *in, *out;
	char c;
	int num_bytes = 0;

	// append the file path with ".backup/"
	char new_path[256];
	strcpy(new_path, ".backup/");
	if (((Node *) node)->path != NULL)
		strcat(new_path, ((Node *) node)->path);
	strcat(new_path, ((Node *) node)->name);

	// append the file name with ".bak"
	char new_name[256];
	strcpy(new_name, new_path);
	strcat(new_name, ".bak");

	// get original file path
	char original_path[256];
	strcpy(original_path, ((Node *) node)->path);
	strcat(original_path, ((Node *) node)->name);

	// if its folder
	if (((Node *) node)->type == 4) {

		// create folder if it doesn't exist
		if (stat(new_path, &st) == -1) {

			printf("[Thread %d] Creating folder %s\n", ((Node *) node)->thread_num, new_path);
			_mkdir(new_path);

		} else {
			return (void *) 1; // return 1 if folder exists
		}	
	}

	// if its file
	if (((Node *) node)->type == 8) {

		// create path if it doesn't exist
		if (stat(new_path, &st) == -1) {
			_mkdir(dirname(new_path));
		}

		// open original file
		in = fopen(original_path, "r");

		if (in == NULL) {
			fprintf(stderr, "Value of errno: %d\n", errno);
			fprintf(stderr, "Error opening in file: %s\n", strerror( errno ));
   		}

		if (stat(new_name, &st) == -1) { // if a backup file doesn't already exist

			// open a new file
			out = fopen(new_name, "w+");

			if (out == NULL) {
				fprintf(stderr, "Value of errno: %d\n", errno);
				fprintf(stderr, "Error opening out file: %s\n", strerror( errno ));
			}

			if (in != NULL && out != NULL) {
				while((c = fgetc(in)) != EOF) { // copy files
					fputc(c, out);
					num_bytes++;
				}
					
				// free memory
				fclose(in);
				fclose(out);

				printf("[Thread %d] Copied %d bytes from %s to %s\n", ((Node *) node)->thread_num, num_bytes, original_path, new_name);
			}
		} else { // if file already exists in backup folder

			time_t t_1 = lastModifiedTime(original_path);
			time_t t_2 = lastModifiedTime(new_name);

			if (t_2 < t_1) { // and file is older than the one we're trying to copy
				printf("[Thread %d] WARNING: overwriting %s\n", ((Node *) node)->thread_num, new_name);

				// open file for overwriting
				out = fopen(new_name, "w+");

				if (out == NULL) {
					fprintf(stderr, "Value of errno: %d\n", errno);
					fprintf(stderr, "Error opening out file: %s\n", strerror( errno ));
				}

				if (in != NULL && out != NULL) {
					while((c = fgetc(in)) != EOF) { // copy file
						fputc(c, out);
						num_bytes++;
					}
		
					// free memory
					fclose(in);
					fclose(out);

					printf("[Thread %d] Copied %d bytes from %s to %s\n", ((Node *) node)->thread_num, num_bytes, original_path, new_name);
				}
			} else {
				return (void *) 1; // return 1 if backup file isn't older
			}
		}
	}

	return (void *) 0;
}

// function recursivley reads files from a directory and adds them to a linked list
Node *read_files(char *current_directory, Node *head) {
	DIR *d;
	struct dirent *dir;
		
	d = opendir(current_directory);
		
	// if you could open the directory
	if (d){

		// for everything in the directory
		while ((dir = readdir(d)) != NULL){

			// that isn't '.', '..' or '.backup'
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".backup") != 0 && strcmp(dir->d_name, "main") != 0) {

				Node *temp = createNode(dir->d_name, dir->d_type);


				// if thing is directory
				if (dir->d_type == 4) {


					// and the process can open the directory

					char new_current_directory[256];
					char new_directory[256];

					strcpy(new_current_directory, current_directory);
					strcpy(new_directory, dir->d_name);
					strcat(new_directory, "/");
					strcat(new_current_directory, new_directory);
					head = read_files(new_current_directory, head);
										
					
					
				} 

				strcpy(temp->path, current_directory);		
				head = push(head, temp); // add file to linked list

			}
		}
		closedir(d); // free memory
	} else {
		printf("Error! Couldn't open directory %s\n", current_directory);
	}
	
	return head;

}

int main(int argc, char *argv[]) {

	Node *head = NULL, *temp;
	int num_files = 0, i = 0;
	void *status = NULL;
	long unsuccessful_files = 0;

	if (argc == 1) { // if backing up

		// create ".backup" folder if it doesn't exist
		if (stat(".backup", &st) == -1) {
			mkdir(".backup", 0777);
		}

		head = read_files("./", head); // make ll of all files

		num_files = getNumFiles(head); // get total number of all files in ll

		temp = head;

		while (temp != NULL) { // for each node in the linked list

			temp->thread_num = i + 1;

			// make a thread
			pthread_create(&fileCopy_thread[i], NULL, copy, (void *) temp);

			i++;
			temp = temp->next;

		}

		for (i = 0; i < num_files; i++) {

			pthread_join(fileCopy_thread[i], &status); // wait for threads to finish
			if ((long) status == 1)
				unsuccessful_files += 1; // file wasn't copied

		}
			
		// print the number of copied files
		printf("Sucessfully copied %d files\n", (int)((long) num_files - unsuccessful_files));

	}

	else if (argc == 2 && strcmp(argv[1],"-r") == 0) { // if restoring files

		head = read_files(".backup/", head); // back ll of ".backup" foler

		num_files = getNumFiles(head); // get total num files in ll

		temp = head;

		while (temp != NULL) { // for each node in ll

			temp->thread_num = i+1;

			// make a thread
			pthread_create(&fileRestore_thread[i], NULL, restore, (void *) temp);

			i++;
			temp = temp->next;

		}

		for (i = 0; i < num_files; i++) {

			pthread_join(fileRestore_thread[i], &status); // wait for threads to finish
			if ((long) status == 1)
				unsuccessful_files += 1; 

		}
				
		printf("Sucessfully restored %d files\n", (int)((long) num_files - unsuccessful_files));
	
	}

	else {
		printf("Invalid arguments, use either ./main or ./main -r\n");
	}

  	return 0;
}
