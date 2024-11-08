#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>

typedef struct {
    hsize_t *shape;
    hid_t dtype;
    size_t offset;
    size_t ndims;
} ArrayMetaData;

ArrayMetaData *arrays_metadata;
size_t metadata_count = 0;

herr_t visit(hid_t loc_id, const char *name, const H5O_info2_t *info, void *op_data) {
    if (info->type == H5O_TYPE_DATASET) {
        hid_t dataset_id = H5Dopen2(loc_id, name, H5P_DEFAULT);
        if (dataset_id < 0) return 0;

        // Get offset and shape
        haddr_t offset = H5Dget_offset(dataset_id);
        if (offset == HADDR_UNDEF) {
            printf("Could not get offset, probably not a continous array\n");
            H5Dclose(dataset_id);
            return 0;
        }

        // Allocate metadata for the array
        arrays_metadata = realloc(arrays_metadata, (metadata_count + 1) * sizeof(ArrayMetaData));
        arrays_metadata[metadata_count].offset = offset;

        // Get shape and type information
        hid_t dataspace = H5Dget_space(dataset_id);
        arrays_metadata[metadata_count].ndims = H5Sget_simple_extent_ndims(dataspace);
        arrays_metadata[metadata_count].shape = malloc(arrays_metadata[metadata_count].ndims * sizeof(hsize_t));
        H5Sget_simple_extent_dims(dataspace, arrays_metadata[metadata_count].shape, NULL);
        arrays_metadata[metadata_count].dtype = H5Dget_type(dataset_id);
        
        printf("%s dataset at offset %zu\n", name, (size_t)offset);

        H5Sclose(dataspace);
        H5Dclose(dataset_id);
        metadata_count++;
    } else {
        printf("%s group\n", name);
    }
    return 0; // Success
}

void *to_array(ArrayMetaData metadata, void *mapping) {
    size_t length = 1;
    for (size_t i = 0; i < metadata.ndims; i++) {
        length *= metadata.shape[i];
    }
    return (void *)((char *)mapping + metadata.offset);
}

int main() {
    const char *path = "grid.h5";
    hid_t h5file = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if(h5file < 0){
        fprintf(stderr, "Error opening file.\n");
        return 1;
    }

    // Iniitalize metadata container
    arrays_metadata = NULL;
    metadata_count = 0;

    // Visit all items
    H5O_info_t info;
    H5Ovisit_by_name(h5file, "/", H5_INDEX_NAME, H5_ITER_NATIVE, visit, NULL, H5O_INFO_ALL, H5P_DEFAULT);
    H5Fclose(h5file);

    fprintf(stdout, " -- Opening file for mmap\n");
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Error opening file for mmap\n");
        return 1;
    }

    fprintf(stdout, " -- memory mapping file\n");
    struct stat sb;
    if(fstat(fd,&sb) == -1) {
        perror("fstat\n");
        close(fd);
        return 1;
    }

    void *mapping = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap\n");
        close(fd);
        return 1;
    }
    close(fd);

    fprintf(stdout, " -- File mapped\n");
    for (size_t i = 0; i < metadata_count; i++) {
        ArrayMetaData meta = arrays_metadata[i];
        printf("Array %zu offset %zu\n", i, meta.offset);
        if (meta.ndims == 2) {
            printf("Shape = [%llu, %llu]\n", (unsigned long long)meta.shape[0], (unsigned long long)meta.shape[1]);
        }

        if ( i == 2){
            void *array = to_array(meta, mapping);
            if (H5Tequal(meta.dtype, H5T_NATIVE_DOUBLE)) {
                double *double_array = (double *)array;
                for (hsize_t row = 0; row < meta.shape[0]; row++) {
                    for (hsize_t col = 0; col < meta.shape[1]; col++) {
                        double value = double_array[row * meta.shape[1] + col];
                        printf("double_array[%llu][%llu] = %lf\n", (unsigned long long)row, (unsigned long long)col, value);
                    }
                    if (row > 10) break;
                }
            } else {
                printf("Unsupported data type.\n");
            }
        }
    }

    return 0;
}
