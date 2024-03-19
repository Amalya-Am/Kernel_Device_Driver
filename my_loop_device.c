#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mm.h>

#define DEVICE_NAME "loop"
#define OUTPUT_FILE "/tmp/output"
#define MAX_CHUNK_SIZE 4096 // Maximum chunk size

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amalya");
MODULE_DESCRIPTION("Loop Device Driver");

static int major_number; //Major number of device
static struct file *output_file;  // File pointer for /tmp/output
static struct class *loop_class; // Class for device creation
static struct device *loop_device; // Device structure for device creation
static struct cdev loop_cdev; // cdev structure


// Function to open loop device 
static int loop_open(struct inode *inode, struct file *file) {
    pr_info("Device opened.\n");
    return 0;
}

// Function to release loop device 
static int loop_release(struct inode *inode, struct file *file) {
    pr_info("Device closed.\n");
    return 0;
}


// Function to calculate chunk size
static size_t calculate_chunk_size(size_t file_size) {
	return min(file_size, (size_t)MAX_CHUNK_SIZE);

}
// Write data to the loop device
static ssize_t loop_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
    struct file *output_file;   // File pointer for the output file
    char *kernel_buffer;        // Kernel buffer to hold data temporarily
    ssize_t retval = -ENOMEM;   // Return value in case of error
    size_t remaining_bytes = count;  // Number of bytes remaining to write
    size_t bytes_written = 0;    // Number of bytes already written
    int i;  // Loop iterator

    // Open the output file in write mode. If the file exist (pos == 0), truncate it.
    // Otherwise, append to the existing content.
    output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | ((*pos == 0) ? O_TRUNC : O_APPEND), 0644);
    if (IS_ERR(output_file)) {
        pr_err("Failed to open output file.\n");
        return PTR_ERR(output_file);
    }
    // Allocate a kernel buffer for streaming
    kernel_buffer = kmalloc(MAX_CHUNK_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        filp_close(output_file, NULL);
        return retval;
    }
    // Calculate chunk size based on input file size
    size_t chunk_size = calculate_chunk_size(count);
    // Loop through input data in chunks and write to output file
    while (remaining_bytes > 0) {
        size_t bytes_to_write = min(remaining_bytes, chunk_size);
        // Copy data from user space to kernel buffer
        if (copy_from_user(kernel_buffer, buf + bytes_written, bytes_to_write)) {
            kfree(kernel_buffer);
            filp_close(output_file, NULL);
            return -EFAULT;
        }
        // Write hex format to the output file
        for (i = 0; i < bytes_to_write; i++) {
            int current_byte = (unsigned char)kernel_buffer[i];
            char hex_value[3];
            sprintf(hex_value, "%02X", current_byte);
            // Write hex value to output file
            kernel_write(output_file, hex_value, 2, &output_file->f_pos);
            // Add newline after every 16 bytes
            if (((i + bytes_written) + 1) % 16 == 0)
                kernel_write(output_file, "\n", 1, &output_file->f_pos);
        }
        // Update bytes written and remaining bytes
        bytes_written += bytes_to_write;
        remaining_bytes -= bytes_to_write;
    }
    // Add a newline character if the last line does not end with one
    if (count % 16 != 0)
        kernel_write(output_file, "\n", 1, &output_file->f_pos);
    // Cleanup
    kfree(kernel_buffer);
    filp_close(output_file, NULL);
    *pos += count;
    return count;
}

// File operations for the loop device
static struct file_operations loop_fops = {
    .owner = THIS_MODULE,
    .open = loop_open,
    .release = loop_release,
    .write = loop_write,
};

// Initialization function for the module
static int __init loop_init(void) {
    pr_info("Module loaded.\n");
    // Create or open the output file
    output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(output_file)) {
        pr_err("Failed to open output file.\n");
        return PTR_ERR(output_file);
    }
    // Allocate a major number dynamically for the character device
    if (alloc_chrdev_region(&major_number, 0, 1, DEVICE_NAME) < 0) {
        pr_err("Failed to allocate major number.\n");
        return -1;
    }
    // Create a class for the device
    static const char *class_name = "loop";
    loop_class = class_create(class_name);

    if (IS_ERR(loop_class)) {
        unregister_chrdev_region(major_number, 1);
        pr_err("Failed to create device class.\n");
        return PTR_ERR(loop_class);
    }
    // Create the device
    loop_device = device_create(loop_class, NULL, major_number, NULL, DEVICE_NAME);
    if (IS_ERR(loop_device)) {
        class_destroy(loop_class);
        unregister_chrdev_region(major_number, 1);
        pr_err("Failed to create device.\n");
        return PTR_ERR(loop_device);
    }
    // Initialize the cdev structure
    cdev_init(&loop_cdev, &loop_fops);
    // Add the cdev to the kernel
    if (cdev_add(&loop_cdev, major_number, 1) < 0) {
        device_destroy(loop_class, major_number);
        class_destroy(loop_class);
        unregister_chrdev_region(major_number, 1);
        pr_err("Failed to add cdev.\n");
        return -1;
    }
    pr_info("Character device registered with major number %d.\n", major_number);

           return 0;
}

static void __exit loop_exit(void) {
    cdev_del(&loop_cdev);
    device_destroy(loop_class, major_number);
    class_destroy(loop_class);
    unregister_chrdev_region(major_number, 1);

    // Close the output file
    if (output_file) {
        filp_close(output_file, NULL);
        pr_info("Output file closed.\n");
    }

    pr_info("Character device unregistered.\n");
}

module_init(loop_init);
module_exit(loop_exit);

