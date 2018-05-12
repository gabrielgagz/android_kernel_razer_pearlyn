/*
 * Copyright (C) 2014 Razer Inc.  All Right Reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * Razer provides this code “as is” and makes no warranties or give any
 * representation of its effectiveness, quality, fitness for any purpose,
 * satisfactory quality or that it is free from any defect or error.
 *
 * Razer shall in no event be liable for any lost profits, loss of information
 * or data, special, incidental, indirect, punitive or consequential or
 * incidental damages, arising in any way out of distribution of, sale of,
 * resale of, use of, or inability to use the code.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kernel.h>
 
typedef struct {
    char vendorID[5];
    char productName[13];
    int manufactureWeek;
    int manufactureYear;
} HDMIEdidInfo;

static HDMIEdidInfo info;

static void hdmi_edid_extract_vendor_id(const u8 *in_buf) {
	u32 id_codes = ((u32)in_buf[8] << 8) + in_buf[9];
    
    info.vendorID[0] = 'A' - 1 + ((id_codes >> 10) & 0x1F);
	info.vendorID[1] = 'A' - 1 + ((id_codes >> 5) & 0x1F);
	info.vendorID[2] = 'A' - 1 + (id_codes & 0x1F);
	info.vendorID[3] = 0;
} 

static void hdmi_edid_extract_product_name(const u8 *in_buf) {
    int i, j;
    
    for(i = 0x36; i < 0x7E; i += 0x12) {
        if(in_buf[i] == 0x00) {
            if(in_buf[i+3] == 0xFC) {
                for(j = 0; j < 13; j++) {
                    if(in_buf[i+5+j] == 0x0A) {
                        info.productName[j] = 0x00;
                    }
                    else {
                        info.productName[j] = in_buf[i+5+j];
                    }
                }
            }
        }
    }
}

void razer_parse_edid(const u8 *edid_buf) {
    hdmi_edid_extract_vendor_id(edid_buf);
    hdmi_edid_extract_product_name(edid_buf);
    info.manufactureWeek = edid_buf[16];
    info.manufactureYear = edid_buf[17] + 1990;
}

EXPORT_SYMBOL(razer_parse_edid);

static int proc_hdmi_edid_info_print(struct seq_file *m, void *v) {
    seq_printf(m, "MANUFACTURER: %s\nPRODUCT: %s\nWEEK: %d\nYEAR: %d\n", info.vendorID, info.productName, info.manufactureWeek, info.manufactureYear);
    return 0;
}

static int proc_hdmi_edid_info_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_hdmi_edid_info_print, NULL);
}

static const struct file_operations proc_hdmi_edid_info_fops = {
    .open       = proc_hdmi_edid_info_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,    
};

static int __init proc_hdmi_edid_info_init(void) {
    proc_create("hdmi_edid_info", 0, NULL, &proc_hdmi_edid_info_fops);
    return 0;
}

static void __exit proc_hdmi_edid_info_exit(void) {
    remove_proc_entry("hdmi_edid_info", NULL);
}

module_init(proc_hdmi_edid_info_init);
module_exit(proc_hdmi_edid_info_exit);
/* Author Information */
MODULE_AUTHOR("Justin Kwok <justin.kwok@razerzone.com>");
MODULE_DESCRIPTION("Razer Driver for HDMI EDID information.");