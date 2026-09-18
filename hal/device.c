#include <hal/device.h>
#include <hal/hal.h>

static jlos_device_t         *s_devices[JLOS_HAL_MAX_DEVICES];
static int                   s_device_count = 0;
static jlos_hal_mmio_track_t s_mmio_track[JLOS_MMIO_TRACK_MAX];

static bool mmio_ranges_overlap(uint32_t a_s, uint32_t a_e, uint32_t b_s, uint32_t b_e)
{
    return !(a_e < b_s || b_e < a_s);
}

static int mmio_claim(uint32_t start, uint32_t end, const char *owner)
{
    if (start > end) return -1;
    for (int i = 0; i < JLOS_MMIO_TRACK_MAX; i++) {
        if (!s_mmio_track[i].used) continue;
        if (mmio_ranges_overlap(start, end, s_mmio_track[i].start, s_mmio_track[i].end)) return -2;
    }
    for (int i = 0; i < JLOS_MMIO_TRACK_MAX; i++) {
        if (s_mmio_track[i].used) continue;
        s_mmio_track[i].used  = true;
        s_mmio_track[i].start = start;
        s_mmio_track[i].end   = end;
        jlos_strlcpy(s_mmio_track[i].owner, owner ? owner : "(null)", JLOS_HAL_MMIO_OWNER_LEN);
        return 0;
    }
    return -3;
}

static void mmio_release(uint32_t start, uint32_t end)
{
    for (int i = 0; i < JLOS_MMIO_TRACK_MAX; i++) {
        if (!s_mmio_track[i].used) continue;
        if (s_mmio_track[i].start == start && s_mmio_track[i].end == end) {
            s_mmio_track[i].used = false;
            s_mmio_track[i].owner[0] = '\0';
            return;
        }
    }
}

int jlos_hal_device_register(jlos_device_t *dev)
{
    if (!dev || !dev->name) return -1;
    if (dev->registered)    return -2;
    if (s_device_count >= JLOS_HAL_MAX_DEVICES) return -3;

    int claimed[JLOS_HAL_MAX_RESOURCES];
    int claimed_count = 0;
    int rc = 0;

    for (int i = 0; i < JLOS_HAL_MAX_RESOURCES; i++) {
        jlos_resource_t *r = &dev->resources[i];
        if (r->type == JLOS_RES_NONE) continue;

        switch (r->type) {
        case JLOS_RES_IO_PORT:
            if (r->start > 0xFFFF || r->end > 0xFFFF || r->start > r->end) { rc = -10; goto rollback; }
            if (jlos_hal_register_io_range((uint16_t)r->start, (uint16_t)r->end, dev->name) != 0) { rc = -11; goto rollback; }
            claimed[claimed_count++] = i;
            break;
        case JLOS_RES_IRQ:
            if (r->start != r->end || r->start > 255) { rc = -20; goto rollback; }
            if (jlos_hal_irq_claim((uint8_t)r->start, dev->name) != 0) { rc = -21; goto rollback; }
            claimed[claimed_count++] = i;
            break;
        case JLOS_RES_MMIO:
            if (r->start > r->end) { rc = -30; goto rollback; }
            if (mmio_claim(r->start, r->end, dev->name) != 0) { rc = -31; goto rollback; }
            claimed[claimed_count++] = i;
            break;
        case JLOS_RES_DMA_CHAN:
            if (r->start != r->end || r->start > 7) { rc = -40; goto rollback; }
            claimed[claimed_count++] = i;
            break;
        default:
            rc = -99;
            goto rollback;
        }
    }

    for (int i = 0; i < JLOS_HAL_MAX_DEVICES; i++) {
        if (s_devices[i]) continue;
        s_devices[i]   = dev;
        dev->registered = true;
        s_device_count++;
        return 0;
    }
    rc = -4;

rollback:
    for (int j = claimed_count - 1; j >= 0; j--) {
        jlos_resource_t *r = &dev->resources[claimed[j]];
        switch (r->type) {
        case JLOS_RES_IO_PORT:
            jlos_hal_unregister_io_range((uint16_t)r->start, (uint16_t)r->end);
            break;
        case JLOS_RES_IRQ:
            jlos_hal_irq_release((uint8_t)r->start);
            break;
        case JLOS_RES_MMIO:
            mmio_release(r->start, r->end);
            break;
        default: break;
        }
    }
    return rc;
}


