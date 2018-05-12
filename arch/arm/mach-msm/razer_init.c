/*
 * board RAZER driver
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/razer_init.h>

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#define RAZER_ITEM_NEXT(curr_item) \
	curr_item = (struct item_t *)((char *)curr_item + sizeof(struct razer_desc) + curr_item->desc.size)

static int proc_razer_read_item(struct file *filp, char *buf, size_t count, loff_t *offp);
                                
static const struct file_operations razer_proc_fops = {
    .owner = THIS_MODULE,
    .read = proc_razer_read_item,
    .write = NULL,
};

/* only for reading/writing razer partition */
#define SERIAL16_SIZE   16
#define RAZER_ATAG_SIZE  2048
unsigned char system_razer[RAZER_ATAG_SIZE+1];

static int proc_razer_read_item(struct file *filp, char *buf, size_t count, loff_t *offp)
{
	struct razer_atag_t *patag_razer = (struct razer_atag_t *)&system_razer[0];
	struct item_atag_t *pitem = NULL;
	unsigned char razer_data[128] = {0};
	int items_num = 10*(patag_razer->items_num[0]-'0') + (patag_razer->items_num[1]-'0');
	int i = 0;
	int item_size, value_len;
	char name[RAZER_MAX_NAME_LEN] = {0};
	char* star;

	static int finished = 0;

	/* 
	 * We return 0 to indicate end of file, that we have
	 * no more information. Otherwise, processes will
	 * continue to read from us in an endless loop. 
	 */
	if ( finished ) {
		finished = 0;
		return 0;
	}
	
	finished = 1;
	
	pitem = (struct item_atag_t *)(&(patag_razer->item_data[0]));
	for (i = 0; i < items_num; i++) {
	    memset(name, 0, RAZER_MAX_NAME_LEN);
	    star = strchr(pitem->data, '*');
	    strncpy(name, pitem->data, star-(char*)pitem->data);
	    item_size = 10*(pitem->size[0]-'0') + (pitem->size[1]-'0');
		if ( 0 == strcmp(filp->f_path.dentry->d_iname, name) ) {
		    value_len = item_size - (star-(char*)pitem->data) - 1;
			strncpy( razer_data, star+1, value_len );
			if (copy_to_user(buf, razer_data, value_len))
			    return -1;
			break;
		}else{
		    pitem = (struct item_atag_t *)((char *)pitem + item_size + 2);
			//RAZER_ITEM_NEXT(pitem);
		}
	}

	return value_len;
}

void create_razer_proc(void)
{
	struct proc_dir_entry *proc_entry;
	struct razer_atag_t *patag_razer = (struct razer_atag_t *)&system_razer[0];
	struct item_atag_t *pitem = NULL;
	int items_num = 10*(patag_razer->items_num[0]-'0') + (patag_razer->items_num[1]-'0');
	int i = 0;
	int item_size;
	char name[RAZER_MAX_NAME_LEN] = {0};
	char* star;


	pitem = (struct item_atag_t *)(&(patag_razer->item_data[0]));
	for (i = 0; i < items_num; i++) {
	    memset(name, 0, RAZER_MAX_NAME_LEN);
	    star = strchr(pitem->data, '*');
	    strncpy(name, pitem->data, star-(char*)pitem->data);
	    printk(KERN_ERR "%s %s\n", __func__, name);
        if ((proc_entry = proc_create(name, 0444, NULL, &razer_proc_fops)) == NULL) {
            return;
        }
        item_size = 10*(pitem->size[0]-'0') + (pitem->size[1]-'0');
		pitem = (struct item_atag_t *)((char *)pitem + item_size + 2);
		//RAZER_ITEM_NEXT(pitem);
	}
}


static int __init razer_init_proc(void)
{
    printk(KERN_ERR "%s check %d\n", __func__, 0);
    create_razer_proc();
	return 0;
}

static int __init razer_setup(char *str)
{
	if (str)
		memcpy(system_razer, str, sizeof(system_razer));
	return 1;
}

__setup("razer=", razer_setup);

module_init(razer_init_proc);


