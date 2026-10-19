/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fs/elf.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "elf"
#include <kernel/printk.h>

static bool elf_check_magic(const uint8_t *ident)
{
    return (ident[0] == JLOS_ELF_MAGIC0 && ident[1] == JLOS_ELF_MAGIC1 &&
        ident[2] == JLOS_ELF_MAGIC2 && ident[3] == JLOS_ELF_MAGIC3);
}

static uint32_t elf_flags_to_vma_flags(uint32_t flags)
{
    uint32_t f = JLOS_VMA_USER;
    if (flags & JLOS_ELF_PF_R) {
        f |= JLOS_VMA_READ;
    }
    if (flags & JLOS_ELF_PF_W) {
        f |= JLOS_VMA_WRITE;
    }
    if (flags & JLOS_ELF_PF_X) {
        f |= JLOS_VMA_EXEC;
    }
    return f;
}

static uint32_t elf_flags_to_pte_flags(uint32_t flags)
{
    if (flags & JLOS_ELF_PF_W) {
        return JLOS_PG_USER_RW;
    }
    return JLOS_PG_USER_RO;
}

static bool elf_load_segment(jlos_mm_t *mm, jlos_vfs_file_t *file,
    uintptr_t vaddr, uint64_t offset, uint64_t filesz, uint64_t memsz, uint32_t flags)
{
    uintptr_t seg_start = JLOS_PAGE_ALIGN_DOWN(vaddr);
    uintptr_t seg_end = JLOS_PAGE_ALIGN_UP(vaddr + memsz);
    uint32_t pte_flags = elf_flags_to_pte_flags(flags);
    uint32_t vma_flags = elf_flags_to_vma_flags(flags);
    uintptr_t file_end = vaddr + filesz;

    if (!jlos_vma_add(mm, seg_start, seg_end, vma_flags, JLOS_VMA_TYPE_FILE)) {
        return false;
    }
    for (uintptr_t page = seg_start; page < seg_end; page += JLOS_PAGE_SIZE) {
        void *frame = jlos_page_frame_malloc();
        if (!frame) {
            return false;
        }
        jlos_memset(frame, 0, JLOS_PAGE_FRAME_SIZE);
        if (!jlos_paging_map(mm->pc, page, VIRT_TO_PHYS(frame), pte_flags)) {
            jlos_page_frame_free(frame);
            return false;
        }
        if (page < file_end && page + JLOS_PAGE_FRAME_SIZE > vaddr) {
            uintptr_t copy_start = page > vaddr ? page : vaddr;
            uintptr_t copy_end = (page + JLOS_PAGE_FRAME_SIZE) < file_end ? (page + JLOS_PAGE_FRAME_SIZE) : file_end;
            size_t copy_size = copy_end - copy_start;
            uint64_t file_offset = offset + (copy_start - vaddr);
            jlos_vfs_lseek(file, file_offset, JLOS_VFS_SEEK_SET);
            if (jlos_vfs_read(file, (uint8_t *)frame + (copy_start - page), copy_size) != (int32_t)copy_size) {
                return false;
            }
        }
    }
    return true;
}

bool jlos_elf_load(jlos_mm_t *mm, jlos_vfs_file_t *file, uintptr_t *entry)
{
#ifdef CONFIG_X86_64
    jlos_elf64_ehdr_t ehdr;
    jlos_vfs_lseek(file, 0, JLOS_VFS_SEEK_SET);
    if (jlos_vfs_read(file, (uint8_t *)&ehdr, sizeof(ehdr)) != (int32_t)sizeof(ehdr)) {
        printk_err("failed to read elf header\n");
        return false;
    }
    if (!elf_check_magic(ehdr.ident)) {
        printk_err("bad elf magic\n");
        return false;
    }
    if (ehdr.type != JLOS_ELF_ET_EXEC) {
        printk_err("not executable\n");
        return false;
    }
    if (ehdr.machine != JLOS_ELF_EM_X86_64) {
        printk_err("not x86_64\n");
        return false;
    }
    for (uint16_t i = 0; i < ehdr.phnum; i++) {
        jlos_elf64_phdr_t phdr;
        jlos_vfs_lseek(file, ehdr.phoff + i * ehdr.phentsize, JLOS_VFS_SEEK_SET);
        if (jlos_vfs_read(file, (uint8_t *)&phdr, sizeof(phdr)) != (int32_t)sizeof(phdr)) {
            printk_err("failed to read phdr %u\n", i);
            return false;
        }
        if (phdr.type != JLOS_ELF_PT_LOAD) {
            continue;
        }
        if (!elf_load_segment(mm, file, phdr.vaddr, phdr.offset, phdr.filesz, phdr.memsz, phdr.flags)) {
            printk_err("failed to load segment %u\n", i);
            return false;
        }
    }
    *entry = ehdr.entry;
#else
    jlos_elf32_ehdr_t ehdr;
    jlos_vfs_lseek(file, 0, JLOS_VFS_SEEK_SET);
    if (jlos_vfs_read(file, (uint8_t *)&ehdr, sizeof(ehdr)) != (int32_t)sizeof(ehdr)) {
        printk_err("failed to read elf header\n");
        return false;
    }
    if (!elf_check_magic(ehdr.ident)) {
        printk_err("bad elf magic\n");
        return false;
    }
    if (ehdr.type != JLOS_ELF_ET_EXEC) {
        printk_err("not executable\n");
        return false;
    }
    if (ehdr.machine != JLOS_ELF_EM_386) {
        printk_err("not i386\n");
        return false;
    }
    for (uint16_t i = 0; i < ehdr.phnum; i++) {
        jlos_elf32_phdr_t phdr;
        jlos_vfs_lseek(file, ehdr.phoff + i * ehdr.phentsize, JLOS_VFS_SEEK_SET);
        if (jlos_vfs_read(file, (uint8_t *)&phdr, sizeof(phdr)) != (int32_t)sizeof(phdr)) {
            printk_err("failed to read phdr %u\n", i);
            return false;
        }
        if (phdr.type != JLOS_ELF_PT_LOAD) {
            continue;
        }
        if (!elf_load_segment(mm, file, phdr.vaddr, phdr.offset, phdr.filesz, phdr.memsz, phdr.flags)) {
            printk_err("failed to load segment %u\n", i);
            return false;
        }
    }
    *entry = ehdr.entry;
#endif
    return true;
}
