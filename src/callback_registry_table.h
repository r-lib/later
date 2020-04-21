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

  // Basically a struct that keeps track of a registry and whether or an R loop
  // object references it.
  class RegistryHandle {
  public:
    RegistryHandle(boost::shared_ptr<CallbackRegistry> registry, bool r_ref_exists)
      : registry(registry), r_ref_exists(r_ref_exists) {
    };
    // Need to declare a copy constructor. Needed because pre-C++11 std::map
    // doesn't have an .emplace() method.
    RegistryHandle() = default;

    boost::shared_ptr<CallbackRegistry> registry;
    bool r_ref_exists;
  };

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

    if (exists(id)) {
      Rcpp::stop("Can't create event loop %d because it already exists.", id);
    }

    // Each new registry is passed our mutex and condvar. These serve as a
    // shared lock across all CallbackRegistries and this
    // CallbackRegistryTable. If each registry had a separate lock, some
    // routines would recursively acquire a lock downward in the
    // CallbackRegistry tree, and some recursively acquire a lock upward;
    // without a shared lock, if these things happen at the same time from
    // different threads, it could deadlock.
    shared_ptr<CallbackRegistry> registry = make_shared<CallbackRegistry>(id, &mutex, &condvar);

    if (parent_id != -1) {
      shared_ptr<CallbackRegistry> parent = getRegistry(parent_id);
      if (parent == nullptr) {
        Rcpp::stop("Can't create registry. Parent with id %d does not exist.", parent_id);
      }
      registry->parent = parent;
      parent->children.push_back(registry);
    }

    // Would be better to use .emplace() to avoid copy-constructor, but that
    // requires C++11.
    registries[id] = RegistryHandle(registry, true);
  }

  // Returns a shared_ptr to the registry. If the registry is not present in
  // the table, or if the target CallbackRegistry has already been deleted,
  // then the shared_ptr is empty.
  shared_ptr<CallbackRegistry> getRegistry(int id) {
    Guard guard(&mutex);
    if (!exists(id)) {
      return shared_ptr<CallbackRegistry>();
    }
    // If the target of the shared_ptr has been deleted, then this is an empty
    // shared_ptr.
    return registries[id].registry;
  }

  uint64_t scheduleCallback(void (*func)(void*), void* data, double delaySecs, int loop_id) {
    // This method can be called from any thread
    Guard guard(&mutex);

    shared_ptr<CallbackRegistry> registry = getRegistry(loop_id);
    if (registry == nullptr) {
      return 0;
    }
    return doExecLater(registry, func, data, delaySecs, true);
  }

  // This is called when the R loop handle referring to a CallbackRegistry is
  // destroyed. Returns true if the CallbackRegistry exists and this function
  // has not previously been called on it; false otherwise.
  bool notifyRRefDeleted(int id) {
    ASSERT_MAIN_THREAD()
    Guard guard(&mutex);

    if (!exists(id)) {
      return false;
    }

    if (registries[id].r_ref_exists) {
      registries[id].r_ref_exists = false;
      this->pruneRegistries();
      return true;
    } else {
      return false;
    }
  }

  // Iterate over all registries, and remove a registry when:
  // * If the loop has a parent:
  //   * There's no R loop object referring to it, AND the registry is empty.
  // * If the loop does not have a parent:
  //   * There's no R loop object referring to it. (Dont' need the registry to
  //     be empty, because if there's no parent and no R reference to the loop,
  //     there is no way to execute callbacks in the registry.)
  void pruneRegistries() {
    ASSERT_MAIN_THREAD()
    Guard guard(&mutex);

    std::map<int, RegistryHandle>::iterator it = registries.begin();

    // Iterate over all registries. Remove under the following conditions:
    // * There are no more R loop handle references to the registry, AND
    // * The registry is empty, OR the registry has no parent.
    // This logic is equivalent to the logic describing the function, just in
    // a different order.
    while (it != registries.end()) {
      if (!it->second.r_ref_exists &&
          (it->second.registry->empty() || it->second.registry->parent == nullptr))
      {
        // Need to increment iterator before removing the registry; otherwise
        // the iterator will be invalid.
        int id = it->first;
        it++;
        remove(id);
      } else {
        it++;
      }
    }
  }

private:
  std::map<int, RegistryHandle> registries;
  Mutex mutex;
  ConditionVariable condvar;

  bool remove(int id) {
    // Removal is always called from the main thread.
    ASSERT_MAIN_THREAD()
    Guard guard(&mutex);

    shared_ptr<CallbackRegistry> registry = getRegistry(id);
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


};


#endif
