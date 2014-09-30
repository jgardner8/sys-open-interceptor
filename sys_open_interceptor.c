#include <linux/module.h>
#include <linux/syscalls.h>

#define P_EXTENSIONS_LEN_MAX 5
#define P_EXTENSIONS_LEN_MAX_STR "5"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Gardner <jamesgardner@live.com.au> "
              "Heavily based on Julia Evans' rickroll.c <https://github.com/jvns/kernel-module-fun/blob/master/rickroll.c>");
MODULE_DESCRIPTION("Any calls to sys_open with the specified extension are intercepted "
                   "and replaced with the specified file.");

static char *p_replacement_filename;
static char *p_extensions[P_EXTENSIONS_LEN_MAX];
static int p_extensions_len;
MODULE_PARM_DESC(p_extensions, "Files with one of these extensions will be replaced in sys_open calls. "
    "Max of " P_EXTENSIONS_LEN_MAX_STR " values.");
module_param_array(p_extensions, charp, &p_extensions_len, 0);
MODULE_PARM_DESC(p_replacement_filename, "Path to file to replace sys_open calls with.");
module_param(p_replacement_filename, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

static ulong **g_sys_call_table;
asmlinkage long (*g_sys_open_original)(const char*, int, umode_t);

static void set_write_protection(bool write_protection) {
    const int WRITE_PROTECT_BIT = 0x10000;
    if (write_protection) {
        write_cr0(read_cr0() | WRITE_PROTECT_BIT);
    } else {
        write_cr0(read_cr0() & (~ WRITE_PROTECT_BIT));
    }
}
 
static ulong **find_sys_call_table(void) {
    ulong **table;
    for (table = (ulong**)PAGE_OFFSET; table < (ulong**)ULLONG_MAX; table += sizeof(void*)) {
        if (table[__NR_close] == (ulong*)sys_close) {
            return table;
        }
    }
    return NULL;
}       
 
static const char *file_extension(const char *filename) {
    int i;
    for (i = strlen(filename)-1; i >= 0; i--) {
        if (filename[i] == '.') {
            return filename + i + 1;
        }
    }
    return "";
}
 
asmlinkage long sys_open_override(const char *filename, int flags, umode_t mode) {
    long file;
    int i;
    for (i = 0; i < p_extensions_len; i++) {
        if (strcmp(file_extension(filename), p_extensions[i]) == 0) {
            mm_segment_t old_fs = get_fs();
            set_fs(KERNEL_DS);
            file = (*g_sys_open_original)(p_replacement_filename, flags, mode);
            set_fs(old_fs);
            printk(KERN_INFO "sys_call_interceptor intercepted %s\n", filename);
            return file;
        }
    }
    return (*g_sys_open_original)(filename, flags, mode);
}
 
static int __init mod_init(void) {
    const int INVALID_ARGUMENT = -EINVAL;
    const int TRY_AGAIN = -EAGAIN;

    if (!p_replacement_filename) {
        printk(KERN_ERR "Missing filename parameter\n");
        return INVALID_ARGUMENT;
    }

    if (!p_extensions_len) {
        printk(KERN_ERR "Missing extensions parameter\n");
        return INVALID_ARGUMENT;
    }
 
    g_sys_call_table = find_sys_call_table();
    if (!g_sys_call_table) {
        printk(KERN_ERR "Couldn't find sys_call_table\n");
        return TRY_AGAIN;
    }

    set_write_protection(false);
    g_sys_open_original = (void*)g_sys_call_table[__NR_open];
    g_sys_call_table[__NR_open] = (ulong*)sys_open_override;
    set_write_protection(true);
 
    printk(KERN_INFO "sys_call_interceptor started\n");
    return 0;
}
 
static void __exit mod_exit(void) {
    set_write_protection(false);
    g_sys_call_table[__NR_open] = (ulong*)g_sys_open_original;
    set_write_protection(true);
 
    printk(KERN_INFO "sys_call_interceptor stopped\n");
}
  
module_init(mod_init);
module_exit(mod_exit);