#ifndef _CALLBACK_REGISTRY_TABLE_H_
#define _CALLBACK_REGISTRY_TABLE_H_

#include <Rcpp.h>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "threadutils.h"
#include "debug.h"
#include "callback_registry.h"
#include "later.h"

using boost::shared_ptr;
using boost::make_shared;

// ============================================================================
// Callback registry table
// ============================================================================
//
// This class is used for accessing a registry by ID. The CallbackRegistries
// also have a tree structure. The global loop/registry is the root. However,
// there can also be trees that are independent of the global loop, if a loop
// is created without a parent.
//
// The operations on this class are thread-safe, because they might be used to
// from another thread.
//
class CallbackRegistryTable {
public:
  CallbackRegistryTable() : mutex(tct_mtx_plain | tct_mtx_recursive), condvar(mutex)  {
  }

  bool exists(int id) {
    Guard guard(&mutex);
    return (registries.find(id) != registries.end());
  }

  // Create a new CallbackRegistry. If parent_id is -1, then there is no parent.
  void create(int id, int parent_id) {
    ASSERT_MAIN_THREAD()
    Guard guard(&mutex);

    // Each new registry is passed our mutex and condvar. These serve as a
    // shared lock across all CallbackRegistries and this
    // CallbackRegistryTable. If each registry had a separate lock, some
    // routines would recursively acquire a lock downward in the
    // CallbackRegistry tree, and some recursively acquire a lock upward;
    // without a shared lock, if these things happen at the same time from
    // different threads, it could deadlock.
    shared_ptr<CallbackRegistry> registry = make_shared<CallbackRegistry>(id, &mutex, &condvar);

    if (exists(id)) {
      Rcpp::stop("Can't create event loop %d because it already exists.", id);
    }

    if (parent_id != -1) {
      shared_ptr<CallbackRegistry> parent = get(parent_id);
      if (parent == nullptr) {
        Rcpp::stop("Can't create registry. Parent with id %d does not exist.", parent_id);
      }
      registry->parent = parent;
      parent->children.push_back(registry);
    }

    registries[id] = shared_ptr<CallbackRegistry>(registry);
  }

  // Returns a shared_ptr to the registry. If the registry is not present in
  // the table, or if the target CallbackRegistry has already been deleted,
  // then the shared_ptr is empty.
  shared_ptr<CallbackRegistry> get(int id) {
    Guard guard(&mutex);
    if (!exists(id)) {
      return shared_ptr<CallbackRegistry>();
    }
    // If the target of the shared_ptr has been deleted, then this is an empty
    // shared_ptr.
    return registries[id];
  }

  bool remove(int id) {
    Guard guard(&mutex);

    shared_ptr<CallbackRegistry> registry = get(id);
    if (registry == nullptr) {
      return false;
    }

    // Deregister this object from its parent. Do it here instead of the in the
    // CallbackRegistry destructor, for two reasons: One is that we can be 100%
    // sure that the deregistration happens right now (it's possible that the
    // object's destructor won't run yet, because someone else has a shared_ptr
    // to it). Second, we can't reliably use a shared_ptr to the object from
    // inside its destructor; we need to some pointer comparison, but by the
    // time the destructor runs, you can't run shared_from_this() in the object,
    // because there are no more shared_ptrs to it.
    shared_ptr<CallbackRegistry> parent = registry->parent;
    if (parent != nullptr) {
      // Remove this registry from the parent's list of children.
      for (std::vector<shared_ptr<CallbackRegistry> >::iterator it = parent->children.begin();
           it != parent->children.end();
          )
      {
        if (*it == registry) {
          parent->children.erase(it);
          break;
        } else {
          ++it;
        }
      }
    }

    // Tell the children that they no longer have a parent.
    for (std::vector<boost::shared_ptr<CallbackRegistry> >::iterator it = registry->children.begin();
         it != registry->children.end();
         ++it)
    {
      (*it)->parent.reset();
    }

    registries.erase(id);

    return true;
  }

private:
  std::map<int, shared_ptr<CallbackRegistry> > registries;
  Mutex mutex;
  ConditionVariable condvar;
};


#endif
