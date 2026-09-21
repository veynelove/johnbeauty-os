#ifndef _JLOS_FILESYSTEM_ELF_H
#define _JLOS_FILESYSTEM_ELF_H

#include <common/types.h>
#include <filesystem/vfs.h>
#include <kernel/vma.h>

#define JLOS_ELF_MAGIC0     0x7F
#define JLOS_ELF_MAGIC1     'E'
#define JLOS_ELF_MAGIC2     'L'
#define JLOS_ELF_MAGIC3     'F'

#define JLOS_ELF_ET_EXEC    2
#define JLOS_ELF_EM_386     3

#define JLOS_ELF_PT_LOAD    1

#define JLOS_ELF_PF_X       1
#define JLOS_ELF_PF_W       2
#define JLOS_ELF_PF_R       4

typedef struct {
    uint8_t     ident[16];
    uint16_t    type;
    uint16_t    machine;
    uint32_t    version;
    uint32_t    entry;
    uint32_t    phoff;
    uint32_t    shoff;
    uint32_t    flags;
    uint16_t    ehsize;
    uint16_t    phentsize;
    uint16_t    phnum;
    uint16_t    shentsize;
    uint16_t    shnum;
    uint16_t    shstrndx;
} __attribute__((packed)) jlos_elf32_ehdr_t;

typedef struct {
    uint32_t    type;
    uint32_t    offset;
    uint32_t    vaddr;
    uint32_t    paddr;
    uint32_t    filesz;
    uint32_t    memsz;
    uint32_t    flags;
    uint32_t    align;
} __attribute__((packed)) jlos_elf32_phdr_t;

bool jlos_elf_load(jlos_mm_t *mm, jlos_vfs_file_t *file, uint32_t *entry);

#endif
