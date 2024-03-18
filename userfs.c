#include "userfs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

			if ((flags & UFS_CREATE) != 0) {
				ufs_error_code = UFS_ERR_NO_FILE;
				return -1;
			}
			break;
		}
		current_file = current_file->next;
	}

	struct file* new_file = malloc(sizeof(struct file));
	if (new_file == NULL) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	new_file->name = strdup(filename);
	new_file->block_list = NULL;
	new_file->last_block = NULL;
	new_file->refs = 1;
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

	ufs_error_code = UFS_ERR_NO_ERR;
	return 1;
}

struct block* 
get_last_block(struct file* file)
{
	if (file == NULL) {
		return NULL;
	}

	struct block* current_block = file->block_list;

	while (current_block != NULL && current_block->next != NULL) {
		current_block = current_block->next;
	}

	return current_block;
}

ssize_t ufs_write(int fd, const char* buf, size_t size)
{
	if (buf == NULL || size == 0) {
		return -1;
	}

	if (fd < 0 || fd >= file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* file = file_descriptors[fd]->file;

	size_t bytes_written = 0;
	struct block* last_block = get_last_block(file);

	while (bytes_written < size) {

		if (last_block == NULL || last_block->occupied == BLOCK_SIZE) {
			struct block* new_block = malloc(sizeof(struct block));
			if (new_block == NULL) {
				ufs_error_code = UFS_ERR_NO_MEM;
				return -1;
			}

			new_block->memory = malloc(BLOCK_SIZE);
			if (new_block->memory == NULL) {
				free(new_block);
				ufs_error_code = UFS_ERR_NO_MEM;
				return -1;
			}

			new_block->occupied = 0;
			new_block->next = NULL;
			new_block->prev = last_block;

			if (last_block == NULL) {
				file->block_list = new_block;
			}
			else {
				last_block->next = new_block;
			}

			file->last_block = new_block;
			last_block = new_block;
		}

		size_t remaining_space = BLOCK_SIZE - last_block->occupied;
		size_t bytes_to_write = size - bytes_written < remaining_space ? size - bytes_written : remaining_space;

		memcpy(last_block->memory + last_block->occupied, buf + bytes_written, bytes_to_write);
		last_block->occupied += bytes_to_write;
		bytes_written += bytes_to_write;
	}

	ufs_error_code = UFS_ERR_NO_ERR;
	return bytes_written;
}

ssize_t 
ufs_read(int fd, char* buf, size_t size)
{
	if (buf == NULL || size == 0) {
		return -1;
	}

	if (fd < 0 || fd >= file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* file = file_descriptors[fd]->file;
	if (file == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	ssize_t bytes_read = 0;
	struct block* current_block = file->block_list;

	while (current_block != NULL && bytes_read < size) {
		size_t remaining_bytes = size - bytes_read;
		size_t bytes_to_read = (remaining_bytes < BLOCK_SIZE) ? remaining_bytes : BLOCK_SIZE;

		memcpy(buf + bytes_read, current_block->memory, bytes_to_read);
		bytes_read += bytes_to_read;

		current_block = current_block->next;
	}

	return bytes_read;
}

int 
ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_count || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;

	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;
}

int ufs_delete(const char* filename)
{
	if (filename == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* current_file = file_list;

	while (current_file != NULL) {
		if (strcmp(current_file->name, filename) == 0) {
			if (current_file->prev != NULL) {
				current_file->prev->next = current_file->next;
			}
			else {
				file_list = current_file->next;
			}

			if (current_file->next != NULL) {
				current_file->next->prev = current_file->prev;
			}

			struct block* current_block = current_file->block_list;

			while (current_block != NULL) {
				struct block* temp = current_block;
				current_block = current_block->next;

				free(temp->memory);
				free(temp);
			}

			free(current_file->name);
			free(current_file);

			ufs_error_code = UFS_ERR_NO_ERR;
			return 0;
		}

		current_file = current_file->next;
	}

	ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

void
ufs_destroy(void)
{
	struct file* current_file = file_list;
	while (current_file != NULL) {
		struct block* current_block = current_file->block_list;
		while (current_block != NULL) {
			struct block* temp = current_block;
			current_block = current_block->next;
			free(temp->memory);
			free(temp);
		}
		free(current_file->name);
		struct file* temp_file = current_file;
		current_file = current_file->next;
		free(temp_file);
	}
}
