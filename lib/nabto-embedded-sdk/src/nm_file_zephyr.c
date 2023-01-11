#include <nabto/nabto_device_zephyr.h>
#include <modules/file/nm_file.h>

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>

static enum nm_file_error create_directory(void* impl, const char* path);
static enum nm_file_error exists(void* impl, const char* path);
static enum nm_file_error size(void* impl, const char* path, size_t* fileSize);
static enum nm_file_error read_file(void* impl, const char* path, void* buffer, size_t bufferSize, size_t* readLength);
static enum nm_file_error write_file(void* impl, const char* path, const uint8_t* content, size_t contentSize);

struct nm_file nm_file_zephyr_get_impl()
{
    struct nm_file impl;
    impl.impl = NULL;
    impl.create_directory = create_directory;
    impl.exists = exists;
    impl.size = size;
    impl.read_file = read_file;
    impl.write_file = write_file;
    return impl;
};

static enum nm_file_error create_directory(void* impl, const char* path)
{
    int ec = fs_mkdir(path);
    if (ec == -EEXIST) {
        return NM_FILE_EXISTS;
    } else if (ec != 0) {
        return NM_FILE_UNKNOWN;
    }
    return NM_FILE_OK;
}

static enum nm_file_error exists(void* impl, const char* path)
{
    struct fs_dirent dirent;
    int ec = fs_stat(path, &dirent);
    if (ec == -ENOENT) {
        return NM_FILE_NO_ENTRY;
    } else if (ec != 0) {
        return NM_FILE_UNKNOWN;
    }
    return NM_FILE_OK;
}

static enum nm_file_error size(void* impl, const char* path, size_t* fileSize)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    int ec = fs_open(&f, path, FS_O_READ);
    if (ec == -ENOENT) {
        return NM_FILE_NO_ENTRY;
    } else if (ec != 0) {
        return NM_FILE_UNKNOWN;
    }
    enum nm_file_error status = NM_FILE_OK;
    ec = fs_seek(&f, 0, FS_SEEK_END);
    if (ec == 0) {
        off_t offset = fs_tell(&f);
        if (offset < 0) {
            status = NM_FILE_UNKNOWN;
        } else {
            *fileSize = offset;
        }
    } else {
        status = NM_FILE_UNKNOWN;
    }

    fs_close(&f);

    return status;
}

static enum nm_file_error read_file(void* impl, const char* path, void* buffer, size_t bufferSize, size_t* readLength)
{
    struct fs_file_t f;
    fs_file_t_init(&f);
    int ec = fs_open(&f, path, FS_O_READ);
    if (ec == -ENOENT) {
        return NM_FILE_NO_ENTRY;
    } else if (ec != 0) {
        return NM_FILE_UNKNOWN;
    }

    enum nm_file_error status = NM_FILE_OK;
    ssize_t r = fs_read(&f, buffer, bufferSize);
    if (r < 0) {
        status = NM_FILE_UNKNOWN;
    } else {
        *readLength = r;
    }
    fs_close(&f);
    return status;
}

static enum nm_file_error write_file(void* impl, const char* path, const uint8_t* content, size_t contentSize)
{
    struct fs_file_t f;
    fs_file_t_init(&f);
    int ec = fs_open(&f, path, FS_O_WRITE | FS_O_CREATE);
    if (ec == -ENOENT) {
        return NM_FILE_NO_ENTRY;
    } else if (ec != 0) {
        return NM_FILE_UNKNOWN;
    }
    enum nm_file_error status = NM_FILE_OK;

    ssize_t written = fs_write(&f, content, contentSize);
    if (written < 0) {
        status = NM_FILE_UNKNOWN;
    }

    if (written != contentSize) {
        status = NM_FILE_TRUNCATED;
    }

    fs_close(&f);
    return status;
}
