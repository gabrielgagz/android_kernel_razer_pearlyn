/*
 * board RAZER driver header file
 */

#ifndef _RAZER_INIT_H_
#define _RAZER_INIT_H_

#define RAZER_MAGIC_NUMBER "4C6F6B69"
#define RAZER_MAX_NAME_LEN 16

struct razer_desc {
    char name[RAZER_MAX_NAME_LEN];
    unsigned int size;
    unsigned int exportable;
  unsigned int permission;
};

struct item_t {
    struct razer_desc desc;
    unsigned char data[1];
};

struct razer_t {
    char magic[8];
    char version[4];
    unsigned int items_num;
    unsigned char item_data[1];
};

struct item_atag_t {
    char size[2];
	unsigned char data[1];
};

struct razer_atag_t {
	char items_num[2];
	unsigned char item_data[1];
};

#endif //_RAZER_INIT_H_
