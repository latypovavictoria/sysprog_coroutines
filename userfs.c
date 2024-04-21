#include "userfs.h"
#include <stddef.h>
#include <stdio.h>
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
	char* memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block* next;
	/** Previous block in the file. */
	struct block* prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block* block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block* last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char* name;
	/** Files are stored in a double-linked list. */
	struct file* next;
	struct file* prev;

	size_t file_size;
	int deleted;

	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file* file_list = NULL;

struct filedesc {
	struct file* file;
	int read_write_flag;
	int block;
	int position;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc** file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code ufs_errno() 
{
	return ufs_error_code;
}

int 
ufs_open(const char* filename, int flags) 
{
	struct file* current_file = file_list;

	while (current_file != NULL && (strcmp(filename, current_file->name) || current_file->deleted)) {
		current_file = current_file->next;
	}

	if (current_file == NULL && !(flags & UFS_CREATE)) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (current_file == NULL) {
		current_file = (struct file*)malloc(sizeof(struct file));
		current_file->block_list = NULL;
		current_file->last_block = NULL;
		current_file->refs = 0;
		current_file->name = strdup(filename);
		current_file->deleted = 0;
		current_file->file_size = 0;
		current_file->next = file_list;
		current_file->prev = NULL;

		if (file_list) {
			file_list->prev = current_file;
		}
		file_list = current_file;
	}

	if (!(flags & UFS_READ_ONLY) && !(flags & UFS_WRITE_ONLY)) {
		flags |= UFS_READ_WRITE;
	}

	if (file_descriptor_count == file_descriptor_capacity) {

		if (!file_descriptors) {
			file_descriptors = (struct filedesc**)malloc(sizeof(struct filedesc*));
			file_descriptor_capacity = 1;

			*file_descriptors = NULL;
			ufs_error_code = UFS_ERR_NO_MEM;
		}
		file_descriptors = (struct filedesc**)realloc(file_descriptors, sizeof(struct filedesc*) * file_descriptor_capacity * 2);
		file_descriptor_capacity *= 2;

		for (int i = file_descriptor_count; i < file_descriptor_capacity; i++) {
			file_descriptors[i] = NULL;
		}
	}

	for (int i = 0; i < file_descriptor_capacity; i++) {
		if (file_descriptors[i] == NULL) {
			struct filedesc* fd = (struct filedesc*)malloc(sizeof(struct filedesc));
			file_descriptors[i] = fd;
			file_descriptor_count++;
			fd->file = current_file;
			current_file->refs++;
			fd->block = 0;
			fd->position = 0;
			fd->read_write_flag = flags;
			return i + 1;
		}
	}

	return -1;
}

ssize_t 
ufs_write(int fd, const char* buf, size_t size) 
{
	size_t real_file_size = 0;

	if (fd < 1 || file_descriptors[fd - 1] == NULL || fd > file_descriptor_capacity + 1) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (buf == NULL) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	struct filedesc* filedesc = file_descriptors[fd - 1];
	struct file* file = filedesc->file;

	if (!(filedesc->read_write_flag & (UFS_WRITE_ONLY | UFS_READ_WRITE))) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}

	if (filedesc->block * BLOCK_SIZE + filedesc->position + size > MAX_FILE_SIZE) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	if (!file->block_list) {
		struct block* block = (struct block*)malloc(sizeof(struct block));
		struct block* prev = NULL;
		block->memory = (char*)malloc(BLOCK_SIZE);
		block->occupied = 0;
		block->prev = prev;
		if (prev) {
			prev->next = block;
		}
		block->next = NULL;
		file->block_list = block;
		file->last_block = block;
	}

	struct block* block = file->block_list;
	for (int i = 0; i < filedesc->block; ++i) {
		block = block->next;
	}
	while (real_file_size < size) {
		if (filedesc->position == BLOCK_SIZE) {
			if (!block->next) {
				struct block* prev_block = block;
				block = (struct block*)malloc(sizeof(struct block));
				block->memory = (char*)malloc(BLOCK_SIZE);
				block->occupied = 0;
				block->prev = prev_block;
				if (prev_block) {
					prev_block->next = block;
				}
				block->next = NULL;
				file->last_block = block;
			}
			else {
				block = block->next;
			}
			filedesc->block++;
			filedesc->position = 0;
		}
		size_t system_size = BLOCK_SIZE - filedesc->position;
		if (size - real_file_size < system_size) {
			system_size = size - real_file_size;
		}
		memcpy(block->memory + filedesc->position, buf + real_file_size, system_size);
		filedesc->position += system_size;
		if (filedesc->position > block->occupied) {
			block->occupied = filedesc->position;
		}
		if ((size_t)(filedesc->position + filedesc->block * BLOCK_SIZE) > file->file_size) {
			file->file_size = filedesc->position + filedesc->block * BLOCK_SIZE;
		}
		real_file_size += system_size;
	}
	return real_file_size;
}

ssize_t 
ufs_read(int fd, char* buf, size_t size) {
	if (fd < 1 || file_descriptors[fd - 1] == NULL || fd > file_descriptor_capacity + 1) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc* filedescriptors = file_descriptors[fd - 1];
	struct file* current_file = filedescriptors->file;

	if (!(filedescriptors->read_write_flag & (UFS_READ_ONLY | UFS_READ_WRITE))) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	size_t bytes_read = 0;
	struct block* block = current_file->block_list;

	for (int i = 0; i < filedescriptors->block; i++) {
		block = block->next;
	}

	while (block && bytes_read < size) {
		if (filedescriptors->position == block->occupied) {
			block = block->next;
			if (block) {
				filedescriptors->block++;
				filedescriptors->position = 0;
			}
		}
		if (!block) {
			return bytes_read;
		}
		size_t system_size = block->occupied - filedescriptors->position;
		if (size - bytes_read < system_size) {
			system_size = size - bytes_read;
		}
		memcpy(buf + bytes_read, block->memory + filedescriptors->position, system_size);
		filedescriptors->position += system_size;
		bytes_read += system_size;
	}

	return bytes_read;
}

int 
ufs_close(int fd) {
	if (fd < 1 || fd > file_descriptor_capacity + 1 || file_descriptors[fd - 1] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct file* current_file = file_descriptors[fd - 1]->file;

	if (!current_file->refs-- && current_file->deleted) {

		if (current_file->next) {
			current_file->next->prev = current_file->prev;
		}
		else if (current_file->prev) {
			current_file->prev->next = current_file->next;
		}
		else if (file_list == current_file) {
			file_list = current_file->next;
		}

		while (current_file->last_block) {

			struct block* temp = current_file->last_block;
			current_file->last_block = current_file->last_block->prev;
			if (current_file->last_block) {
				current_file->last_block->next = NULL;
			}
			free(temp->memory);
			free(temp);

		}
		free(current_file->name);
		free(current_file);
	}
	free(file_descriptors[fd - 1]);
	file_descriptor_count--;
	file_descriptors[fd - 1] = NULL;

	return 0;
}

int 
ufs_delete(const char* filename) 
{
	struct file* current_file = file_list;
	while (current_file != NULL && (strcmp(filename, current_file->name) || current_file->deleted)) {
		current_file = current_file->next;
	}

	if (!current_file) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	current_file->deleted = 1;
	if (!current_file->refs) {

		if (current_file->next) {
			current_file->next->prev = current_file->prev;
		}
		if (current_file->prev) {
			current_file->prev->next = current_file->next;
		}
		if (file_list == current_file) {
			file_list = current_file->next;
		}
		while (current_file->last_block) {
			struct block* temp = current_file->last_block;
			current_file->last_block = current_file->last_block->prev;
			if (current_file->last_block) {
				current_file->last_block->next = NULL;
			}
			free(temp->memory);
			free(temp);
		}
		free(current_file->name);
		free(current_file);
	}

	return 0;
}

#ifdef NEED_RESIZE
int 
ufs_resize(int fd, size_t new_size) 
{
	if (fd < 1 || fd > file_descriptor_capacity + 1 || file_descriptors[fd - 1] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct filedesc* filedescriptor = file_descriptors[fd - 1];
	struct file* current_file = filedescriptor->file;

	if (!(filedescriptor->read_write_flag & (UFS_WRITE_ONLY | UFS_READ_WRITE))) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	if (new_size > MAX_FILE_SIZE) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}

	int new_block = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int new_position = new_size % BLOCK_SIZE;
	while (current_file->file_size > new_size) {
		if (current_file->file_size - current_file->last_block->occupied > new_size) {
			current_file->file_size -= current_file->last_block->occupied;

			struct block* tmp = current_file->last_block;
			current_file->last_block = current_file->last_block->prev;
			if (current_file->last_block) {
				current_file->last_block->next = NULL;
			}
			free(tmp->memory);
			free(tmp);
		}
		else {
			current_file->last_block->occupied = new_position;
			current_file->file_size = new_size;
		}
	}
	if (!new_size) {
		current_file->block_list = NULL;
	}
	while (current_file->file_size < (size_t)new_size) {
		if (current_file->file_size - current_file->last_block->occupied + BLOCK_SIZE < new_size) {
			current_file->file_size -= current_file->last_block->occupied;
			current_file->last_block->occupied = BLOCK_SIZE;
			current_file->file_size += current_file->last_block->occupied;

			struct block* prev = current_file->last_block;
			current_file->last_block = (struct block*)malloc(sizeof(struct block));
			current_file->last_block->memory = (char*)malloc(BLOCK_SIZE);
			current_file->last_block->occupied = 0;
			if (prev) {
				prev->next = current_file->last_block;
			}
			current_file->last_block->next = NULL;

		}
		else {
			current_file->last_block->occupied = new_position;
			current_file->file_size = new_size;
		}
	}
	for (int i = 0; i < file_descriptor_capacity; ++i) {
		struct filedesc* temp = file_descriptors[i];
		if (temp && temp->file == current_file) {
			if ((size_t)(temp->block * BLOCK_SIZE + temp->position) > new_size) {
				temp->block = new_block - 1;
				temp->position = new_position;
			}
		}
	}
	return 0;
}
#endif

void 
ufs_destroy(void) 
{
	struct file* file = file_list;
	while (file) {
		struct file* current_file = file;
		file = file->next;

		if (current_file->next) {
			current_file->next->prev = current_file->prev;
		}
		if (current_file->prev) {
			current_file->prev->next = current_file->next;
		}
		if (file_list == current_file) {
			file_list = current_file->next;
		}

		while (current_file->last_block) {
			struct block* temp_block = current_file->last_block;
			current_file->last_block = current_file->last_block->prev;
			if (current_file->last_block) {
				current_file->last_block->next = NULL;
			}
			free(temp_block->memory);
			free(temp_block);
		}
		free(current_file->name);
		free(current_file);
	}

	for (int i = 0; i < file_descriptor_capacity; ++i) {
		if (file_descriptors[i]) {
			free(file_descriptors[i]);
		}
	}

	free(file_descriptors);
}
