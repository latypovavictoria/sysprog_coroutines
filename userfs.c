#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "userfs.h"
#include <stddef.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char* filename, int flags)
{
	if (filename == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* current_file = file_list;
	while (current_file != NULL) {
		if (strcmp(current_file->name, filename) == 0) {
			current_file->refs++;
			return current_file->refs;
		}
		current_file = current_file->next;
	}

	if ((flags & UFS_CREATE) == 0) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* new_file = malloc(sizeof(struct file));
	new_file->block_list = NULL;
	new_file->last_block = NULL;
	new_file->refs = 1;
	new_file->name = strdup(filename);
	new_file->next = NULL;
	new_file->prev = NULL;

	if (file_list == NULL) {
		file_list = new_file;
	}
	else {
		struct file* last_file = file_list;

		while (last_file->next != NULL) {
			last_file = last_file->next;
		}
		last_file->next = new_file;
		new_file->prev = last_file;
	}
	for (int i = 0; i < file_descriptor_capacity; ++i) {
		if (file_descriptors[i] == NULL) {
			struct filedesc *fd = malloc(sizeof(struct filedesc));
			file_descriptors[i] = fd;
			file_descriptor_count++;
			fd->file = new_file;
			new_file->refs++;
		}
	}
	
	return new_file->refs;
}

ssize_t
ufs_write(int fd, const char* buf, size_t size)
{
	if (fd<0 || fd>file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	

	if (buf == NULL) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	struct file* file = file_descriptors[fd]->file;
	struct block* last_block = file->last_block;

	struct block* new_block = malloc(sizeof(struct block));
	new_block->memory = malloc(BLOCK_SIZE);
	memcpy(new_block->memory, buf, size);
	new_block->occupied = size;
	new_block->next = NULL;
	new_block->prev = last_block;

	file->last_block = new_block;

	if (last_block != NULL) {
		last_block->next = new_block;
	}
	else {
		file->block_list = new_block;
	}

	return size;
}

ssize_t
ufs_read(int fd, char* buf, size_t size)
{
	if (fd<0 || fd>file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* current_file = file_descriptors[fd]->file;
	struct block* current_block = current_file->last_block;

	size_t bytes_read = 0;
	while (current_block != NULL && bytes_read < size) {
		size_t bytes_to_read = size - bytes_read;

		if (bytes_to_read > (size_t)current_block->occupied) {
			bytes_to_read = current_block->occupied;
		}
		memcpy(buf + bytes_read, current_block->memory, bytes_to_read);
		bytes_read += bytes_to_read;
		current_block = current_block->next;
	}

	return bytes_read;
}

int
ufs_close(int fd)
{
	if (fd<0 || fd>file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	file_descriptors[fd]->file->refs--;

	if (file_descriptors[fd]->file->refs == 0) {
		struct file* current_file = file_descriptors[fd]->file;

		while (current_file->block_list != NULL) {
			struct block* temp = current_file->block_list;
			current_file->block_list = current_file->block_list->next;

			free(temp->memory);
			free(temp);
		}
		free(current_file->name);

		if (current_file->prev != NULL) {
			current_file->prev->next = current_file->next;
		}
		else {
			file_list = current_file->next;
		}

		if (current_file->next != NULL) {
			current_file->next->prev = current_file->prev;
		}
		free(current_file);
	}
	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;

	return 0;
}

int
ufs_delete(const char *filename)
{
	if (filename == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* current_file = file_list;
	while (current_file != NULL) {
		if (strcmp(current_file->name, filename) == 0) {
			break;
		}
		current_file = current_file->next;
	}

	if (current_file == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (current_file->prev != NULL) {
		current_file->prev->next = current_file->next;
	}
	else {
		file_list = current_file->next;
	}
	if (current_file->next != NULL) {
		current_file->next->prev = current_file->prev;
	}

	while (current_file->block_list != NULL) {
		struct block* temp = current_file->block_list;
		current_file->block_list = current_file->block_list->next;

		free(temp->memory);
		free(temp);
	}

	free(current_file->name);
	free(current_file);

	return 0;
}

void
ufs_destroy(void)
{
	struct file* current_file = file_list;
	while (current_file != NULL) {
		struct file* temp = current_file;
		current_file = current_file->next;

		while (temp->block_list != NULL) {
			struct block* temp_block = temp->block_list;
			temp->block_list = temp->block_list->next;
			free(temp_block->memory);
			free(temp_block);
		}

		file_list = NULL;

		for (int i = 0; i < file_descriptor_count; i++) {
			if (file_descriptors[i] != NULL) {
				free(file_descriptors[i]);
			}
		}
	}
	free(file_descriptors);
	file_descriptors = NULL;
	file_descriptor_count = 0;
	file_descriptor_capacity = 0;
}
