////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_filesys.c
//  Description    : This is the implementation of the Lion Cloud device
//                   filesystem interfaces.
//
//   Author        : Tzu Chieh Huang
//   Last Modified : Apr 29th 2020
//

// Include files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>

// Project include files
#include <lcloud_filesys.h>
#include <lcloud_cache.h>
#include <lcloud_controller.h>
#include <lcloud_network.h>

typedef struct block* Block;
struct block {
    int sector;
    int block;
    int device_id;
};

typedef struct file* File;
// struct the things need in a file
struct file {
    // Use cache to replace these:
    // char buff[LC_DEVICE_BLOCK_SIZE];
    // int buff_block;
    // int buff_changed;
    int is_open;
    char* file_name;
    int file_size;
    int cur_pos;
    Block blocks;
    int blocks_count;
};
//struct the device in the file
typedef struct device* Device;
struct device {
    int lcloud;
    // to count the lcreg, files, current sector, block and device
    // struct file* files;
    // int files_count;
    int cur_sector;
    int cur_block;
    int sectors_count;
    int blocks_count;
};

int lcloud;
// there are maximum 16 devices
struct device devices[16];
int cur_device;
struct file* files;
int files_count;

// File system interface implementation

/*

LCloudRegisterFrame

Bits   Register
------ --------------------------------------------------
   0-3 B0 - first 4 bit register
   4-7 B1 - second 4 bit register
  8-15 C0 - first 8 bit register
 16-23 C1 - second 8 bit register
 24-31 C2 - third 8 bit register
 32-47 D0 - first 16 bit register
 48-63 D1 - second 16 bit register

 */

LCloudRegisterFrame create_lcloud_registers(int b0, int b1, int c0, int c1,
    int c2, int d0, int d1)
{
    LCloudRegisterFrame lcloud_reg = 0;
    lcloud_reg |= ((LCloudRegisterFrame)b0) << 60;
    lcloud_reg |= ((LCloudRegisterFrame)b1) << 56;
    lcloud_reg |= ((LCloudRegisterFrame)c0) << 48;
    lcloud_reg |= ((LCloudRegisterFrame)c1) << 40;
    lcloud_reg |= ((LCloudRegisterFrame)c2) << 32;
    lcloud_reg |= ((LCloudRegisterFrame)d0) << 16;
    lcloud_reg |= ((LCloudRegisterFrame)d1);
    return lcloud_reg;
}

void extract_lcloud_registers(LCloudRegisterFrame lcloud_reg, int* b0, int* b1,
    int* c0, int* c1, int* c2, int* d0, int* d1)
{
    // address end in 0xffffffff
    // if each register has memory in it, extract it, returning to the original address
    if (b0)
        *b0 = (int)((lcloud_reg >> 60) & 0xf);
    if (b1)
        *b1 = (int)((lcloud_reg >> 56) & 0xf);
    if (c0)
        *c0 = (int)((lcloud_reg >> 48) & 0xff);
    if (c1)
        *c1 = (int)((lcloud_reg >> 40) & 0xff);
    if (c2)
        *c2 = (int)((lcloud_reg >> 32) & 0xff);
    if (d0)
        *d0 = (int)((lcloud_reg >> 16) & 0xffff);
    if (d1)
        *d1 = (int)((lcloud_reg)&0xffff);
}

// Function     : lcloud_io_succeed
// Description  : add the command
//
// Inputs       : lcloud_reg
// Outputs      : b0 && b1 =1, command succesful
int lcloud_io_succeed(LCloudRegisterFrame lcloud_reg)
{
    int b0, b1;
    extract_lcloud_registers(lcloud_reg, &b0, &b1, NULL, NULL, NULL, NULL, NULL);
    return b0 == 1 && b1 == 1;
}

// Function     : lcloud_io_device
// Description  : return the device
// Inputs       : lcloud
// Outputs      : device ids (d0)
int lcloud_io_device(LCloudRegisterFrame lcloud_reg)
{
    int d0;
    extract_lcloud_registers(lcloud_reg, NULL, NULL, NULL, NULL, NULL, &d0, NULL);
    return d0;
}

// Function     : lcloud_io_power_on()
// Description  : power on the icloud register
// Outputs      : 1 if success or 0 if failure
int lcloud_io_power_on()
{
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(0, 0, LC_POWER_ON, 0, 0, 0, 0);
    return lcloud_io_succeed(client_lcloud_bus_request(lcloud_reg, NULL));
}

// Function     : lcloud_io_power_off
// Description  : turn off
// Outputs      : 1 if success or 0 if failure
int lcloud_io_power_off()
{
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(0, 0, LC_POWER_OFF, 0, 0, 0, 0);
    return lcloud_io_succeed(client_lcloud_bus_request(lcloud_reg, NULL));
}

// Function     : lcloud_io_devices_probe
// Description  : check if the device is existed or not
//
// Inputs       : device
// Outputs      : return 1 success, 0 if failure
int lcloud_io_devices_probe(int* device_ids)
{
    // create a lcloud_reg to return 1
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(0, 0, LC_DEVPROBE, 0, 0, 0, 0);
    lcloud_reg = client_lcloud_bus_request(lcloud_reg, NULL);
    if (lcloud_io_succeed(lcloud_reg)) {
        *device_ids = lcloud_io_device(lcloud_reg);
        logMessage(LOG_OUTPUT_LEVEL, "Device Id %d", *device_ids);
        return 1;
    }
    return 0;
}

// Function     : lcloud_io_read
// Description  :reading command. Read the file
//
// Inputs       : device, sector, block, buf
// Outputs      : 1 if success ( b1 & b0 == 1) or 0 if failure
int lcloud_io_read(int device, int sector, int block, char* buf)
{
    // use logMessage to debug and offer the message
    logMessage(LOG_OUTPUT_LEVEL, "Read: device:%d sector:%d, block:%d,", device,
        sector, block);
    // add the detail into the register
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(
        0, 0, LC_BLOCK_XFER, device, LC_XFER_READ, sector, block);

    // return to command
    return lcloud_io_succeed(client_lcloud_bus_request(lcloud_reg, buf));
}

// Function     : lcloud_io_write
// Description  : writing command. input data to the file
//		  (same as the lcloud_io_read)
// Inputs       : device, sector, block, *buf
// Outputs      : file handle if successful test, 0 if failure
int lcloud_io_write(int device, int sector, int block, char* buf)
{
    // debug, getting the message
    logMessage(LOG_OUTPUT_LEVEL, "Write: device:%d sector:%d, block:%d,", device,
        sector, block);
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(
        0, 0, LC_BLOCK_XFER, device, LC_XFER_WRITE, sector, block);
    return lcloud_io_succeed(client_lcloud_bus_request(lcloud_reg, buf));
}

// Function     : lcloud_io_device_init
// Description  : initialize device
// Inputs       : single device id
// Outputs      : 1 if the device are successfully initialized, 0 if failure
int lcloud_io_device_init(int device_id)
{
    LCloudRegisterFrame lcloud_reg = create_lcloud_registers(0, 0, LC_DEVINIT, device_id, 0, 0, 0);
    lcloud_reg = client_lcloud_bus_request(lcloud_reg, NULL);
    if (lcloud_io_succeed(lcloud_reg)) {
        // initialize the device
        int d0, d1;
        extract_lcloud_registers(lcloud_reg, NULL, NULL, NULL, NULL, NULL, &d0, &d1);
        devices[device_id].cur_sector = 0;
        devices[device_id].cur_block = 0;
        devices[device_id].lcloud = 1;
        devices[device_id].sectors_count = d0;
        devices[device_id].blocks_count = d1;
        logMessage(LOG_OUTPUT_LEVEL, "Device %d initialized", device_id);
        return 1;
    } else {
        return 0;
    }
}

// Function     : lcloud_init_devices
// Description  : initialize devices
// Inputs       : device ids from devices probe
// Outputs      : 1 if all devices are successfully initialized, 0 if failure
int lcloud_init_devices(int device_ids)
{
    int dev = 0;

    while (device_ids) {
        if (device_ids & 0x1) {
            if (!lcloud_io_device_init(dev)) {
                logMessage(LOG_OUTPUT_LEVEL, "Device %d init failed", dev);
                return 0;
            }
        }
        device_ids >>= 1;
        dev++;
    }
    return 1;
}

// Function     : lcloud_initialization()
// Description  : get the empty lcreg and initialize the cache
// Outputs      : 1 if success or 0 if failure
int lcloud_initialization()
{
    // if power is not on or no device exist, return failed
    if (!lcloud_io_power_on()) {
        logMessage(LOG_OUTPUT_LEVEL, "Power on failed");
        return 0;
    }
    //initialize the chache
    lcloud_initcache(LC_CACHE_MAXBLOCKS);
    //set up the device memory
    memset(devices, 0, sizeof(devices));
    cur_device = 0;

    int device_ids;
    if (!lcloud_io_devices_probe(&device_ids)) {
        logMessage(LOG_OUTPUT_LEVEL, "Device probe failed");
        return 0;
    }

    if (!lcloud_init_devices(device_ids)) {
        logMessage(LOG_OUTPUT_LEVEL, "Device init failed");
        return 0;
    }

    lcloud = 1;

    logMessage(LOG_OUTPUT_LEVEL, "Initialized");
    return 1;
}

// Function     : lcloud_get_free_block
// Description  : get a new free block
// Inputs       : sector, block
// Outputs      : 1 if success or 0 if failure
int lcloud_get_free_block(int* device_id, int* sector, int* block)
{
    while (cur_device < 16) {
        if (devices[cur_device].cur_sector < devices[cur_device].sectors_count) {
            *device_id = cur_device;
            // find the right location. since we need to get new block, cur_block++
            *sector = devices[cur_device].cur_sector;
            *block = devices[cur_device].cur_block++;
            if (devices[cur_device].cur_block == devices[cur_device].blocks_count) {
                devices[cur_device].cur_sector++;
                devices[cur_device].cur_block = 0;
            }
            return 1;
        }
        cur_device++;
    }
    // if all blocks on all devices are used up, then no space.
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcopen
// Description  : Open the device in file for for reading and writing
//
// Inputs       : path - the path/filename of the file to be read
// Outputs      : file handle i, -1 if failure
LcFHandle lcopen(const char* path)
{
    // since we have more files and more device, we use device to find the specific location
    int i;
    // if there is no register, create a lcreg and cache
    if (lcloud == 0) {
        lcloud_initialization();
    }

    //determine if the files are openned
    for (i = 0; i < files_count; i++) {
        if (strcmp(path, files[i].file_name) == 0) {
            if (files[i].is_open) {
                logMessage(LOG_OUTPUT_LEVEL, "Already open");
                return -1;
            }
            files[i].cur_pos = 0;

            //return the file i
            return i;
        }
    }

    files = (File)realloc(files, sizeof(struct file) * (files_count + 1));
    // open the file
    files[i].file_name = strdup(path);
    files[i].is_open = 1;
    files[i].file_size = 0;
    files[i].cur_pos = 0;
    files[i].blocks = NULL;
    files[i].blocks_count = 0;
    files_count++;
    logMessage(LOG_OUTPUT_LEVEL, "File %d created", i);
    // return the file handle
    return i;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcread
// Description  : Read data from the file
//
// Inputs       : fh - file handle for the file to read from
//                buf - place to put the data
//                len - the length of the read
// Outputs      : number of bytes read, -1 if failure
int lcread(LcFHandle fh, char* buf, size_t len)
{
    char* cache;
    char tmp[LC_DEVICE_BLOCK_SIZE];
    int i, dev, sec, blk;

    // if the file is not valid or not opened, return -1
    if (fh < 0 || fh >= files_count) {
        logMessage(LOG_OUTPUT_LEVEL, "Invalid fh");
        return -1;
    }
    if (!files[fh].is_open) {
        logMessage(LOG_OUTPUT_LEVEL, "Not open");
        return -1;
    }
    // set up the length to read
    size_t left = files[fh].file_size - files[fh].cur_pos;
    if (len > left) {
        len = left;
    }
    // read the lenght n, alread read length n_read
    unsigned int n_read = 0;
    unsigned int n;
    while (n_read < len) {
        // since write and read cannot over the length of block size
        // mod the current position by the block size (to get the length)
        unsigned int begin = files[fh].cur_pos % LC_DEVICE_BLOCK_SIZE;
        n = LC_DEVICE_BLOCK_SIZE - begin;
        if (n > len - n_read) {
            n = len - n_read;
        }

        i = files[fh].cur_pos / LC_DEVICE_BLOCK_SIZE;
        dev = files[fh].blocks[i].device_id;
        sec = files[fh].blocks[i].sector;
        blk = files[fh].blocks[i].block;
	//read the cache
        cache = lcloud_getcache(dev, sec, blk);
        if (!cache) {
            lcloud_io_read(dev, sec, blk, tmp);
            lcloud_putcache(dev, sec, blk, tmp);
            cache = tmp;
        }

        // copy the file's memory to buffer
        memcpy(buf + n_read, cache + begin, n);

        logMessage(LOG_OUTPUT_LEVEL, "write: %.*s", n, buf + n_read);
        files[fh].cur_pos += n;
        n_read += n;
    }
    // return the reading bytes
    return n_read;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcwrite
// Description  : write data to the file
//
// Inputs       : fh - file handle for the file to write to
//                buf - pointer to data to write
//                len - the length of the write
// Outputs      : number of bytes written if successful test, -1 if failure

int lcwrite(LcFHandle fh, char* buf, size_t len)
{
    char* cache;
    char tmp[LC_DEVICE_BLOCK_SIZE];
    int i, dev, sec, blk;

    // check if the file is avalible and valid or not
    if (fh < 0 || fh >= files_count) {
        logMessage(LOG_OUTPUT_LEVEL, "Invalid fh");
        return -1;
    }
    if (!files[fh].is_open) {
        logMessage(LOG_OUTPUT_LEVEL, "Not open");
        return -1;
    }

    // write the file (same as the read)
    unsigned int n_write = 0;
    unsigned int n;
    while (n_write < len) {
        // since write and read cannot over the length of block size
        // mod the current position by the block size (to get the length)
        unsigned int begin = files[fh].cur_pos % LC_DEVICE_BLOCK_SIZE;
        n = LC_DEVICE_BLOCK_SIZE - begin;
        if (n > len - n_write) {
            n = len - n_write;
        }

        i = files[fh].cur_pos / LC_DEVICE_BLOCK_SIZE;

        if (i == files[fh].blocks_count) {
            files[fh].blocks_count++;
            files[fh].blocks = realloc(files[fh].blocks, sizeof(struct block) * files[fh].blocks_count);
            if (!lcloud_get_free_block(&files[fh].blocks[i].device_id, &files[fh].blocks[i].sector, &files[fh].blocks[i].block)) {
                break;
            }
        }
        dev = files[fh].blocks[i].device_id;
        sec = files[fh].blocks[i].sector;
        blk = files[fh].blocks[i].block;

        cache = lcloud_getcache(dev, sec, blk);
        // write through cache
        if (cache) {
            memcpy(cache + begin, buf + n_write, n);
            lcloud_io_write(dev, sec, blk, cache);
        } else {
            lcloud_io_read(dev, sec, blk, tmp);
            memcpy(tmp + begin, buf + n_write, n);
            lcloud_putcache(dev, sec, blk, tmp);
            lcloud_io_write(dev, sec, blk, tmp);
        }

        files[fh].cur_pos += n;
        if (files[fh].cur_pos > files[fh].file_size) {
            files[fh].file_size = files[fh].cur_pos;
        }

        n_write += n;
    }
    // return the bytes
    return n_write;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcseek
// Description  : Seek to a specific place in the file
//
// Inputs       : fh - the file handle of the file to seek in
//                off - offset within the file to seek to
// Outputs      : 0 if successful test, -1 if failure

int lcseek(LcFHandle fh, size_t off)
{
    // check if the position is right and the file is valid or not
    if (fh < 0 || fh >= files_count) {
        logMessage(LOG_OUTPUT_LEVEL, "Invalid fh");
        return -1;
    }
    if (!files[fh].is_open) {
        logMessage(LOG_OUTPUT_LEVEL, "Not open");
        return -1;
    }
    if (files[fh].file_size < off) {
        logMessage(LOG_OUTPUT_LEVEL, "Out of range");
        return -1;
    }
    files[fh].cur_pos = off;
    return (off);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcclose
// Description  : Close the file
//
// Inputs       : fh - the file handle of the file to close
// Outputs      : 0 if successful test, -1 if failure

int lcclose(LcFHandle fh)
{
    if (fh < 0 || fh >= files_count) {
        logMessage(LOG_OUTPUT_LEVEL, "Invalid fh");
        return -1;
    }
    if (!files[fh].is_open) {
        logMessage(LOG_OUTPUT_LEVEL, "Not open");
        return -1;
    }
    // close the file, make the position and is_open into 0
    files[fh].cur_pos = 0;
    files[fh].is_open = 0;
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcshutdown
// Description  : Shut down the filesystem
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int lcshutdown(void)
{
    // if the power is off, return -1
    if (!lcloud_io_power_off()) {
        return -1;
    }
    // clean the file(return to NULL)
    char buf[LC_DEVICE_BLOCK_SIZE * 10];
    memset(buf, 0, LC_DEVICE_BLOCK_SIZE * 10);
    for (int i = 0; i < files_count; i++) {
        free(files[i].file_name);
        free(files[i].blocks);
        files[i].file_name = NULL;
        files[i].blocks = NULL;
    }
    lcloud_closecache();
    return (0);
}
