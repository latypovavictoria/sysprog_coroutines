#include <stdio.h>
#include <stdlib.h>

#include "sort.h"

void get_array_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }
    
    int* array = (int*)malloc(sizeof(int));;
    int count = 0;
    int number = 0;
    while (fscanf(file, "%d", &number) == 1) {
        array = (int*)realloc(array, (count + 1) * sizeof(int));
        array[count++] = number;
    }
    fclose(file);
   
    merge_sort(array, 0, count - 1);
    
    file = fopen(filename, "w");
    for(unsigned int i = 0; i < count; i++) {
    	fprintf(file, "%d ", array[i]);
    }
    fclose(file);
    
}


void merge_sort(int* array, int left, int right) {
    if (left == right) {
        return;
    }

    else {
        int middle = (left + right) / 2;
        merge_sort(array, left, middle);
        merge_sort(array, middle + 1, right);
        
        unsigned int l_bound = left;
        unsigned int r_bound = middle + 1;
        int* temp = (int*)malloc(sizeof(int) * (right - left + 1));
        for (unsigned int step = 0; step < right - left + 1; step++) {

            if ((r_bound > right) || ((l_bound <= middle) && (array[l_bound] < array[r_bound]))) {
                temp[step] = array[l_bound];
                l_bound++;
            }

            else {
                temp[step] = array[r_bound];
                r_bound++;
            }
        }

        for (unsigned int step = 0; step < right - left + 1; step++) {
            array[left + step] = temp[step];
        }
        free(temp);
    }
}
