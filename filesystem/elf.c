#include <filesystem/elf.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "elf"
#include <kernel/printk.h>

static bool elf_check_magic(const jlos_elf32_ehdr_t *ehdr)
{
    return (ehdr->ident[0] == JLOS_ELF_MAGIC0 && ehdr->ident[1] == JLOS_ELF_MAGIC1 &&
        ehdr->ident[2] == JLOS_ELF_MAGIC2 && ehdr->ident[3] == JLOS_ELF_MAGIC3);
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

static bool elf_load_segment(jlos_mm_t *mm, jlos_vfs_file_t *file, const jlos_elf32_phdr_t *phdr)
{
    uint32_t seg_start = JLOS_PAGE_ALIGN_DOWN(phdr->vaddr);
    uint32_t seg_end = JLOS_PAGE_ALIGN_UP(phdr->vaddr + phdr->memsz);
    uint32_t pte_flags = elf_flags_to_pte_flags(phdr->flags);
    uint32_t vma_flags = elf_flags_to_vma_flags(phdr->flags);
    uint32_t file_end = phdr->vaddr + phdr->filesz;

    if (!jlos_vma_add(mm, seg_start, seg_end, vma_flags, JLOS_VMA_TYPE_FILE)) {
        return false;
    }
    for (uint32_t page = seg_start; page < seg_end; page += JLOS_PAGE_SIZE) {
        void *frame = jlos_page_frame_malloc();
        if (!frame) {
            return false;
        }
        jlos_memset(frame, 0, JLOS_PAGE_FRAME_SIZE);
        if (!jlos_paging_map(mm->pc, page, VIRT_TO_PHYS(frame), pte_flags)) {
            jlos_page_frame_free(frame);
            return false;
        }
        if (page < file_end && page + JLOS_PAGE_FRAME_SIZE > phdr->vaddr) {
            uint32_t copy_start = page > phdr->vaddr ? page : phdr->vaddr;
            uint32_t copy_end = (page + JLOS_PAGE_FRAME_SIZE) < file_end ? (page + JLOS_PAGE_FRAME_SIZE) : file_end;
            uint32_t copy_size = copy_end - copy_start;
            uint32_t file_offset = phdr->offset + (copy_start - phdr->vaddr);
            jlos_vfs_lseek(file, file_offset, JLOS_VFS_SEEK_SET);
            if (jlos_vfs_read(file, (uint8_t *)frame + (copy_start - page), copy_size) != (int32_t)copy_size) {
                return false;
            }
        }
    }
    return true;
}

bool jlos_elf_load(jlos_mm_t *mm, jlos_vfs_file_t *file, uint32_t *entry)
{
    jlos_elf32_ehdr_t ehdr;
    jlos_vfs_lseek(file, 0, JLOS_VFS_SEEK_SET);
    if (jlos_vfs_read(file, (uint8_t *)&ehdr, sizeof(ehdr)) != (int32_t)sizeof(ehdr)) {
        printk_err("failed to read elf header\n");
        return false;
    }
    if (!elf_check_magic(&ehdr)) {
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
        if (!elf_load_segment(mm, file, &phdr)) {
            printk_err("failed to load segment %u\n", i);
            return false;
        }
    }
    *entry = ehdr.entry;
    return true;
}
