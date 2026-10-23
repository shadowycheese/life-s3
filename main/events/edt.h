#ifndef EDT_h
#define EDT_h

#include <functional>
#include "esp_log.h"

typedef enum : uint8_t
{
    EVENT_COUNT,
} event_type_t;

typedef struct
{
    event_type_t event_type;
} edt_job_t;

using event_handler_t = std::function<void(edt_job_t)>;

#ifdef __cplusplus
extern "C"
{
#endif
    extern void edt_init();
    extern void edt_post(edt_job_t job);

    extern bool edt_add_game_event_handler(event_type_t event_type, event_handler_t cb);

#ifdef __cplusplus
}
#endif

#endif