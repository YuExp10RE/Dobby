#include "dobby_internal.h"
#include "core/arch/Cpu.h"
#include "PlatformUnifiedInterface/ExecMemory/ClearCacheTool.h"
#include "UnifiedInterface/platform.h"

#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/mman.h>
#include "UnifiedInterface/platform-darwin/mach_vm.h"
#endif

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach/vm_statistics.h>
#endif

PUBLIC MemoryOperationError CodePatch(void *address, uint8_t *buffer, uint32_t buffer_size) {
  if (address == nullptr || buffer == nullptr || buffer_size == 0) {
    FATAL("invalid argument");
    return kMemoryOperationError;
  }

  kern_return_t kr;

  int page_size = PAGE_SIZE;
  addr_t page_aligned_address = ALIGN_FLOOR(address, page_size);
  int offset = (int)((addr_t)address - page_aligned_address);

  vm_map_t self_task = kernel_map;

  addr_t remap_dummy_page = 0;
  kr = vm_allocate(self_task, &remap_dummy_page, page_size, 0);
  if (kr != KERN_SUCCESS)
    return kMemoryOperationError;

  // copy original page
  memcpy((void *)remap_dummy_page, (void *)page_aligned_address, page_size);

  // patch buffer
  memcpy((void *)(remap_dummy_page + offset), buffer, buffer_size);

  // change permission
  kr = vm_protect(kernel_map, remap_dummy_page, page_size, false, VM_PROT_READ | VM_PROT_WRITE);
  if (kr != KERN_SUCCESS)
    return kMemoryOperationError;

  vm_address_t remap_dest_page = page_aligned_address;
  vm_prot_t curr_protection, max_protection;
  kr = vm_remap(self_task, &remap_dest_page, page_size, 0,
                VM_FLAGS_OVERWRITE | VM_FLAGS_FIXED, self_task, remap_dummy_page, TRUE,
                &curr_protection, &max_protection, VM_INHERIT_COPY);
  if (kr != KERN_SUCCESS) {
    return kMemoryOperationError;
  }

  kr = vm_deallocate(self_task, remap_dummy_page, page_size);
  if (kr != KERN_SUCCESS) {
    return kMemoryOperationError;
  }

  ClearCache(address, (void *)((addr_t)address + buffer_size));

  return kMemoryOperationSuccess;

}
