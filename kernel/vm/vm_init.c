#include <kernel/kmem.h>
#include <kernel/arch/pmap.h>
#include <kernel/vm/vm_map.h>
#include <kernel/vm/vm_km.h>
#include <kernel/vm/vm_init.h>

vaddr_t vm_page_array_va;
extern paddr_t kernel_physical_start;
extern paddr_t kernel_physical_end;

void vm_init(void) {
    pmap_bootstrap();
    vm_page_bootstrap(vm_page_array_va);

    // FIXME switching UART to VA address
    extern unsigned long uart_base_addr;
    vaddr_t uart_base_va = 0xFFFFFFFFC0000000;
    pmap_kenter_pa(uart_base_va, uart_base_addr & ~(PAGESIZE - 1), VM_PROT_DEFAULT, PMAP_FLAGS_READ | PMAP_FLAGS_WRITE | PMAP_FLAGS_NOCACHE);
    uart_base_addr = uart_base_va + (uart_base_addr & (PAGESIZE - 1));

    vm_page_init();
    vm_object_bootstrap(kernel_physical_start, kernel_physical_end);
    pmap_init();
    vm_map_init();
    vm_km_init();
    kmem_init();
}