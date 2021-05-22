#include "psi_memory_resource.hpp"

#include "sql/current_thd.h"
#include "sql/sql_class.h"

PSI_memory_key KEY_mem_keyring;

void *psi_memory_resource::do_allocate(std::size_t bytes,
                                       std::size_t /*alignment*/) {
  if (current_thd) {
    PSI_thread *owner = thd_get_psi(current_thd);
    psi_memory_service->memory_alloc(KEY_mem_keyring, bytes, &owner);
  }
  return new char[bytes];
}

void psi_memory_resource::do_deallocate(void *p, std::size_t bytes,
                                        std::size_t /*alignment*/) {
  if (current_thd) {
    PSI_thread *owner = thd_get_psi(current_thd);
    psi_memory_service->memory_free(KEY_mem_keyring, bytes, owner);
  }
  delete[] static_cast<char *>(p);
}
