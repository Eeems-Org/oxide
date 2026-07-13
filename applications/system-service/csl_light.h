#pragma once
#ifdef __aarch64__
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <liboxide/debug.h>

namespace csl {
  namespace light {
    enum class Source {
      Keyboard = 0,
      Frontlight = 1,
      Funkey = 2,
    };

    class Manager {
    public:
      Manager(Source source)
        : m_lib(dlopen("libcsllight.so.0", RTLD_LAZY)) {
        if (m_lib == nullptr) {
          return;
        }
#define getFunc(prop, name, signature)                                         \
  prop = reinterpret_cast<signature>(dlsym(m_lib, #name));                     \
  if (prop == nullptr) {                                                       \
    O_WARNING(#name " not found");                                             \
    dlclose(m_lib);                                                            \
    m_lib = nullptr;                                                           \
    return;                                                                    \
  }
        getFunc(
          m_constructor,
          _ZN3csl5light7ManagerC1ENS0_6SourceE,
          void (*)(void*, int)
        );
        getFunc(m_destructor, _ZN3csl5light7ManagerD1Ev, void (*)(void*));
        getFunc(
          m_setBrightness,
          _ZN3csl5light7Manager13setBrightnessEi,
          void (*)(void*, int)
        );
        getFunc(
          m_getBrightness, _ZNK3csl5light7Manager10brightnessEv, int (*)(void*)
        );
        getFunc(
          m_setEnabled,
          _ZN3csl5light7Manager10setEnabledEb,
          void (*)(void*, bool)
        );
        getFunc(
          m_isEnabled, _ZNK3csl5light7Manager9isEnabledEv, bool (*)(void*)
        );
        getFunc(
          m_setBoosted,
          _ZN3csl5light7Manager20setBoostedFrontlightEb,
          void (*)(void*, bool)
        );
        getFunc(
          m_isBoosted,
          _ZN3csl5light7Manager26isBoostedFrontlightEnabledEv,
          bool (*)(void*)
        );
#undef getFunc
        m_object = std::malloc(128);
        if (m_object == nullptr) {
          O_WARNING("Failed to malloc object");
          dlclose(m_lib);
          m_lib = nullptr;
          return;
        }
        std::memset(m_object, 0, 128);
        m_constructor(m_object, static_cast<int>(source));
      }

      ~Manager() {
        if (m_object != nullptr && m_destructor != nullptr) {
          m_destructor(m_object);
        }
        if (m_object != nullptr) {
          std::free(m_object);
        }
        if (m_lib != nullptr) {
          dlclose(m_lib);
        }
      }

      bool isValid() const {
        return m_lib != nullptr && m_object != nullptr;
      }

      void setBrightness(int brightness) {
        if (isValid()) {
          m_setBrightness(m_object, brightness);
        }
      }

      int brightness() const {
        return isValid() ? m_getBrightness(m_object) : 0;
      }

      void setEnabled(bool enabled) {
        if (isValid()) {
          m_setEnabled(m_object, enabled);
        }
      }

      bool isEnabled() const {
        return isValid() && m_isEnabled(m_object);
      }

      void setBoostedFrontlight(bool boosted) {
        if (isValid()) {
          m_setBoosted(m_object, boosted);
        }
      }

      bool isBoostedFrontlightEnabled() {
        return isValid() && m_isBoosted(m_object);
      }

    private:
      void* m_lib = nullptr;
      void* m_object = nullptr;
      void (*m_constructor)(void*, int) = nullptr;
      void (*m_destructor)(void*) = nullptr;
      void (*m_setBrightness)(void*, int) = nullptr;
      int (*m_getBrightness)(void*) = nullptr;
      void (*m_setEnabled)(void*, bool) = nullptr;
      bool (*m_isEnabled)(void*) = nullptr;
      void (*m_setBoosted)(void*, bool) = nullptr;
      bool (*m_isBoosted)(void*) = nullptr;
    };
  }
}
#endif
