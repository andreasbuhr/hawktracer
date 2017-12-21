#include "hawktracer/alloc.h"
#include "hawktracer/bag.h"
#include "hawktracer/core_events.h"
#include "internal/timeline_klass.h"
#include "internal/registry.h"

#include <assert.h>
#include <string.h>

static HT_Bag timeline_klass_register;
static HT_Bag event_klass_register;

static uint32_t
djb2_hash(const char *str)
{
    uint32_t hash = 5381;
    int c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

void
ht_registry_init(void)
{
    ht_bag_init(&timeline_klass_register, 8);
    ht_bag_init(&event_klass_register, 8);
}

HT_TimelineKlass*
ht_registry_find_timeline_class(const char* klass_id)
{
    uint32_t id;
    size_t i;

    assert(klass_id != NULL);

    id = djb2_hash(klass_id);

    for (i = 0; i < timeline_klass_register.size; i++)
    {
        HT_TimelineKlass* klass = (HT_TimelineKlass*)timeline_klass_register.data[i];
        if (ht_timeline_klass_get_id(klass) == id)
        {
            return klass;
        }
    }

    return NULL;
}

HT_Boolean ht_registry_register_timeline(
        const char* klass_id,
        size_t type_size,
        HT_Boolean shared_listeners,
        void (*init)(HT_Timeline*, va_list),
        void (*deinit)(HT_Timeline*))
{
    void* klass;

    if (ht_registry_find_timeline_class(klass_id) != NULL)
    {
        return HT_FALSE;
    }

    klass = ht_timeline_klass_create(djb2_hash(klass_id), type_size,
                                          shared_listeners, init, deinit);

    ht_bag_add(&timeline_klass_register, klass);

    return HT_TRUE;
}

HT_Boolean
ht_registry_register_event_klass(HT_EventKlass* event_klass)
{
    if (event_klass->type > 0)
    {
        return HT_FALSE;
    }

    ht_bag_add(&event_klass_register, event_klass);
    event_klass->type = event_klass_register.size;

    return HT_TRUE;
}

void
ht_registry_push_all_klass_info_events(HT_Timeline* timeline)
{
    for (size_t i = 0; i < event_klass_register.size; i++)
    {
        ht_registry_push_klass_info_event(timeline, (HT_EventKlass*)event_klass_register.data[i]);
    }

    ht_timeline_flush(timeline);
}

void
ht_registry_push_klass_info_event(HT_Timeline* timeline, HT_EventKlass* klass)
{
    HT_DECL_EVENT(HT_EventKlassInfoEvent, event);
    ht_timeline_init_event(timeline, HT_EVENT(&event));
    event.event_klass_name = klass->type_info->name;
    event.event_type = klass->type;
    event.field_count = klass->type_info->fields_count;

    ht_timeline_push_event(timeline, HT_EVENT(&event));

    for (size_t j = 0; j < klass->type_info->fields_count; j++)
    {
        MKCREFLECT_FieldInfo* info = &klass->type_info->fields[j];
        HT_DECL_EVENT(HT_EventKlassFieldInfoEvent, field_event);
        ht_timeline_init_event(timeline, HT_EVENT(&field_event));
        field_event.data_type = info->data_type;
        field_event.event_type = event.event_type;
        field_event.field_name = info->field_name;
        field_event.field_type = info->field_type;
        field_event.size = info->size;

        ht_timeline_push_event(timeline, HT_EVENT(&field_event));
    }
}

HT_EventKlass**
ht_registry_get_event_klasses(size_t* out_klass_count)
{
    assert(out_klass_count != NULL);

    *out_klass_count = event_klass_register.size;
    return (HT_EventKlass**)event_klass_register.data;
}

void
htregistry_unregister_all_timelines(void)
{
    size_t i;

    for (i = 0; i < timeline_klass_register.size; i++)
    {
        HT_TimelineKlass* klass = (HT_TimelineKlass*)timeline_klass_register.data[i];
        ht_timeline_klass_unref(klass);
    }

    ht_bag_deinit(&timeline_klass_register);
}

void
htregistry_unregister_all_event_klasses(void)
{
    ht_bag_deinit(&event_klass_register);
}