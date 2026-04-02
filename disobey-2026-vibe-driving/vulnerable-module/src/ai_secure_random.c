#include "linux/mutex.h"
#include "linux/types.h"
#include <asm/current.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex_types.h>
#include <linux/random.h>
#include <linux/thread_info.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("alphanine");
MODULE_DESCRIPTION("Secure AI-generated random number generator");
MODULE_ALIAS("ai_secure_random");

// Vulnerable stuff
struct mutex random_buffer_lock;
// Note: this leaks fs_struct which lets you traverse fs
struct task_struct *last_accessor_task;
char random_buffer[256];

// Break ASLR of module just in case
char *random_buffer_pointer;

// Break kernel ASLR
void (*get_random_bytes_func)(void *, size_t);

// Forward declarations
static void ai_secure_random_refresh_random_buffer(void);
static int ai_secure_random_open(struct inode *inode, struct file *file);
static int ai_secure_random_release(struct inode *inode, struct file *file);
static ssize_t ai_secure_random_read(struct file *file, char __user *buf,
                                     size_t count, loff_t *offset);
static loff_t ai_secure_random_llseek(struct file *file, loff_t off, int);

// Linux boilerplate

static const struct file_operations ai_secure_random_fops = {
    .owner = THIS_MODULE,
    .open = ai_secure_random_open,
    .release = ai_secure_random_release,
    .read = ai_secure_random_read,
    .llseek = ai_secure_random_llseek};

static dev_t dev = 0;
static struct class *ai_secure_random_class = NULL;
static struct cdev ai_secure_random_cdev;

#define MAX_RANDOM_READ_SIZE 8

static int ai_secure_random_open(struct inode *inode, struct file *file) {
  return 0;
}

static int ai_secure_random_release(struct inode *inode, struct file *file) {
  return 0;
}

static ssize_t ai_secure_random_read(struct file *file, char __user *buf,
                                     size_t count, loff_t *offset) {
  size_t read_size = count;

  if(read_size >= MAX_RANDOM_READ_SIZE) {
      read_size = MAX_RANDOM_READ_SIZE;
  }

  mutex_lock(&random_buffer_lock);

  last_accessor_task = get_current();
  ai_secure_random_refresh_random_buffer();

  read_size -= copy_to_user(buf, random_buffer_pointer, read_size);

  mutex_unlock(&random_buffer_lock);

  return read_size;
}

static loff_t ai_secure_random_llseek(struct file *file, loff_t off,
                                      int whence) {

  if(whence != 0) {
      return -EINVAL;
  }
  mutex_lock(&random_buffer_lock);

  random_buffer_pointer = (char*)(random_buffer) + off;

  mutex_unlock(&random_buffer_lock);

  return 0;
}

// Refresh random state
// This requires random_buffer_lock to be locked
static void ai_secure_random_refresh_random_buffer(void) {
  get_random_bytes_func(random_buffer, sizeof(random_buffer));
  for (size_t i = 0u; i < sizeof(random_buffer); i++) {
    random_buffer[i] ^= 69;
  }
}

static int __init ai_secure_random_init(void) {
  random_buffer_pointer = random_buffer;
  mutex_init(&random_buffer_lock);

  get_random_bytes_func = get_random_bytes;

  // Initial buffer fill
  ai_secure_random_refresh_random_buffer();

  // No error handling, live on the edge, fuck boilerplate
  alloc_chrdev_region(&dev, 0, 1, "ai_secure_random");

  cdev_init(&ai_secure_random_cdev, &ai_secure_random_fops);

  cdev_add(&ai_secure_random_cdev, dev, 1);

  ai_secure_random_class = class_create("ai_secure_random_class");

  device_create(ai_secure_random_class, NULL, dev, NULL, "ai_secure_random");

  printk(KERN_INFO "AI-powered Copilot random initialized...\n");
  return 0;
}

static void __exit ai_secure_random_exit(void) {
  cdev_del(&ai_secure_random_cdev);
  device_destroy(ai_secure_random_class, dev);
  class_destroy(ai_secure_random_class);
  unregister_chrdev_region(dev, 1);

  printk(KERN_INFO "AI-powered Copilot random uninitialized...\n");
}

module_init(ai_secure_random_init);
module_exit(ai_secure_random_exit);
