#ifndef MYSQLPP_PSI_MEMORY_RESOURCE_HPP
#define MYSQLPP_PSI_MEMORY_RESOURCE_HPP

#include "psi_memory_resource_fwd.hpp"

#include <mysql/components/services/psi_memory.h>
#include <boost/container/pmr/memory_resource.hpp>

class psi_memory_resource : public boost::container::pmr::memory_resource {
 protected:
  virtual void *do_allocate(std::size_t bytes,
                            std::size_t /*alignment*/) override;

  virtual void do_deallocate(void *p, std::size_t bytes,
                             std::size_t /*alignment*/) override;

  virtual bool do_is_equal(const boost::container::pmr::memory_resource &other)
      const noexcept override {
    return &other == this;
  }
};

#endif
