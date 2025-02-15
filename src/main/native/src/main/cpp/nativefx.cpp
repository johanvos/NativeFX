#ifndef _Included_NativeFX_CPP
#define _Included_NativeFX_CPP

// force boost to be  included as header only, also on windows
#define BOOST_ALL_NO_LIB 1

#include<vector>
#include<string>

#include <boost/thread/xtime.hpp>

#include "eu_mihosoft_nativefx_NativeBinding.h"
#include "jnitypeconverter.h"

#include "shared_memory.h"

namespace ipc = boost::interprocess;

std::vector<std::string> names;
std::vector<shared_memory_info*>   connections;
std::vector<void*> buffers;

std::vector<ipc::shared_memory_object*> shm_infos;
std::vector<ipc::mapped_region*> info_regions;
std::vector<ipc::shared_memory_object*> shm_buffers;
std::vector<ipc::mapped_region*> buffer_regions;

JNIEXPORT jstring JNICALL Java_eu_mihosoft_nativefx_NativeBinding_sendMsg
  (JNIEnv *env, jclass cls, jint key, jstring jmsg) {

      shared_memory_info* info_data = NULL;

      if(key >= connections.size()) {
        return stringC2J(env, "ERROR: key not available");
      }

      info_data = connections[key];

      std::string msg = stringJ2C(env, jmsg);

      // send a message to server
      store_shared_string(msg, info_data->client_to_server_msg);
      info_data->client_to_server_msg_semaphore.post();

      // return result from server
      info_data->client_to_server_res_semaphore.wait();
      return stringC2J(env, info_data->client_to_server_res);
}

JNIEXPORT jint JNICALL Java_eu_mihosoft_nativefx_NativeBinding_nextKey
  (JNIEnv *env, jclass cls) {
    return connections.size();
}

void update_buffer_connection(int key) {
  
    if(key >= connections.size()) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return;
    }

      std::string name = names[key];
      std::string info_name = get_info_name(key, name);
      std::string buffer_name = get_buffer_name(key,name);

      try {
        // create a shared memory object.
        ipc::shared_memory_object* shm_buffer = 
          new ipc::shared_memory_object
                (ipc::open_only,             // only open
                 buffer_name.c_str(),        // name
                 ipc::read_write             // read-write mode
        );

        if(shm_buffers[key]!=NULL) {
          delete shm_buffers[key];
        }

        shm_buffers[key] = shm_buffer;

        // map the whole shared memory in this process
        ipc::mapped_region* buffer_region = 
          new ipc::mapped_region(
                 *shm_buffer,    // what to map
                  ipc::read_write     // map it as read-write
        );

        if(buffer_regions[key]!=NULL) {
          delete buffer_regions[key];
        }

        buffer_regions[key] = buffer_region;

        // get the address of the mapped region
        void* buffer_addr = buffer_region->get_address();

        buffers[key] = buffer_addr;

      } catch(...) {

        std::cerr << "ERROR: cannot connect to '" << info_name << "'. Server probably not running." << std::endl;

        return;
      }
}

JNIEXPORT jint JNICALL Java_eu_mihosoft_nativefx_NativeBinding_connectTo
  (JNIEnv *env, jclass cls, jstring jname) {

      using namespace boost::interprocess;
      std::string name = stringJ2C(env, jname);

      // setup key and names for new connection
      int key = connections.size();
      std::string info_name = get_info_name(key, name);
      std::string buffer_name = get_buffer_name(key,name);
      names.push_back(name);

      try {

        // open the shared memory object.
        shared_memory_object* shm_info = 
          new shared_memory_object(open_only,                // only open (don't create)
            info_name.c_str(),                               // name
            read_write                                       // read-write mode
        );

        shm_infos.push_back(shm_info);

        // map the whole shared memory in this process
        mapped_region* info_region = new mapped_region
                (*shm_info,                       // What to map
                  read_write                      // Map it as read-write
        );

        info_regions.push_back(info_region);

        // get the address of the mapped region
        void * info_addr = info_region->get_address();

        // construct the shared structure in memory
        shared_memory_info * info_data = static_cast<shared_memory_info*>(info_addr);
        connections.push_back(info_data);

        info_data->mutex.lock();

        // create a shared memory object.
        shared_memory_object* shm_buffer = 
          new shared_memory_object
                (open_only,             // only open
                 buffer_name.c_str(),   // name
                 read_write             // read-write mode
        );

        shm_buffers.push_back(shm_buffer);

        // map the whole shared memory in this process
        mapped_region* buffer_region = 
          new mapped_region(
                 *shm_buffer,    // What to map
                  read_write     // Map it as read-write
        );

        buffer_regions.push_back(buffer_region);

        // get the address of the mapped region
        void* buffer_addr = buffer_region->get_address();

        buffers.push_back(buffer_addr);

        info_data->mutex.unlock();
      } catch(...) {

        std::cerr << "ERROR: cannot connect to '" << info_name << "'. Server probably not running." << std::endl;

        return -1;
      }

      return key;
}

JNIEXPORT jboolean JNICALL Java_eu_mihosoft_nativefx_NativeBinding_isConnected
  (JNIEnv *env, jclass cls, jint key) {

    namespace ipc = boost::interprocess;
    // try
    // {
    //     if(key >= connections.size()) {
    //       return false;
    //     }

    //     ipc::shared_memory_object shm_info(ipc::open_only, get_info_name(key, names[key]).c_str(),
    //        ipc::read_only
    //     );

    //     return true;
    // } 
    // catch (const std::exception &ex) {
    //     //
    // }
    // return false;

    return boolC2J(key < connections.size() && connections[key] != NULL);
}

JNIEXPORT jboolean JNICALL Java_eu_mihosoft_nativefx_NativeBinding_terminate
  (JNIEnv *env, jclass cls, jint key) {

    if(key >= connections.size()) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return false;
    }

    ipc::shared_memory_object::remove(get_info_name  (key, names[key]).c_str());
    ipc::shared_memory_object::remove(get_buffer_name(key, names[key]).c_str());

    names[key]          = ""; // NULL?

    delete connections[key];
    // delete buffers[key];
    delete shm_infos[key];
    delete info_regions[key];
    delete shm_buffers[key];
    delete buffer_regions[key];

    connections[key]    = NULL;
    buffers[key]        = NULL;
    shm_infos[key]      = NULL;
    info_regions[key]   = NULL;
    shm_buffers[key]    = NULL;
    buffer_regions[key] = NULL;

    return boolC2J(true);
}

JNIEXPORT jobject JNICALL Java_eu_mihosoft_nativefx_NativeBinding_getBuffer
  (JNIEnv *env, jclass cls, jint key) {

  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return NULL;
  }

  update_buffer_connection(key);

  void* buffer_addr = buffers[key];

  jobject result = env->NewDirectByteBuffer(buffer_addr,
     connections[key]->w * connections[key]->h * 4);
  
  return result;
}

JNIEXPORT jint JNICALL Java_eu_mihosoft_nativefx_NativeBinding_getW
  (JNIEnv *env, jclass cls, jint key) {
  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return -1;
  }

  return connections[key]->w;
}

JNIEXPORT jint JNICALL Java_eu_mihosoft_nativefx_NativeBinding_getH
  (JNIEnv *env, jclass cls, jint key) {
  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return -1;
  }

  return connections[key]->h;
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_resize
  (JNIEnv *env, jclass cls, jint key, jint w, jint h) {

  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
      return;
  }

  int prev_w = connections[key]->w;
  int prev_h = connections[key]->h;

  connections[key]->w = w;  
  connections[key]->h = h;  

  if(prev_w != w || prev_h != h) {
    connections[key]->buffer_ready = false;
  }
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_waitForBufferChanges
  (JNIEnv *env, jclass cls, jint key) {

  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    
    // connections[key]->buffer_semaphore.wait();

    // try at least every second 
    // boost::system_time const timeout=
    //   boost::get_system_time()+ boost::posix_time::milliseconds(1000);
    // while(!connections[key]->buffer_semaphore.timed_wait(timeout)) {
    while(!connections[key]->buffer_semaphore.try_wait()) {  
      //
    }
  }

}

JNIEXPORT jboolean JNICALL Java_eu_mihosoft_nativefx_NativeBinding_hasBufferChanges
  (JNIEnv *env, jclass cls, jint key) {

  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    
    // connections[key]->buffer_semaphore.wait();

    // try at least every second 
    // boost::system_time const timeout=
    //   boost::get_system_time()+ boost::posix_time::milliseconds(1000);
    // while(!connections[key]->buffer_semaphore.timed_wait(timeout)) {
    return boolC2J(connections[key]->buffer_semaphore.try_wait());
  }

  return false;
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_lock
  (JNIEnv *env, jclass cls, jint key) {
  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    connections[key]->mutex.lock();
  }
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_unlock
  (JNIEnv *env, jclass cls, jint key) {
  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    connections[key]->mutex.unlock();
  }
}

JNIEXPORT jboolean JNICALL Java_eu_mihosoft_nativefx_NativeBinding_isDirty
  (JNIEnv *env, jclass cls, jint key) {
  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    return boolC2J(connections[key]->dirty);
  }

  return false;
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_setDirty
  (JNIEnv *env, jclass cls, jint key, jboolean dirty) {
  if(key >= connections.size() || connections[key] == NULL) {
    std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    connections[key]->dirty = boolJ2C(dirty);
  }
}

JNIEXPORT void JNICALL Java_eu_mihosoft_nativefx_NativeBinding_setBufferReady
  (JNIEnv *env, jclass cls, jint key, jboolean value) {
  if(key >= connections.size() || connections[key] == NULL) {
    std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    connections[key]->buffer_ready = boolJ2C(value);
  }  
}

JNIEXPORT jboolean JNICALL Java_eu_mihosoft_nativefx_NativeBinding_isBufferReady
  (JNIEnv *env, jclass cls, jint key) {

  if(key >= connections.size() || connections[key] == NULL) {
      std::cerr << "ERROR: key not available: " << key << std::endl;
  } else {
    return boolC2J(connections[key]->buffer_ready);
  }

  return false;

}

#endif /*_Included_NativeFX_CPP*/