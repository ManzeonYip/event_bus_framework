#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 事件类型定义 */
typedef uint32_t EventType;

/* 事件数据结构，业务可扩展 */
typedef struct {
    EventType type;
    size_t size;
    void* data;
} Event;

/* 订阅者回调函数类型 */
typedef void (*SubscriberCallback)(const Event* event, void* user_data);

/* 订阅句柄，用于取消订阅 */
typedef struct SubscriberHandle* SubscriberHandle;

/* 初始化事件总线 */
void event_bus_init(void);

/* 销毁事件总线 */
void event_bus_destroy(void);

/* 订阅事件 */
SubscriberHandle event_bus_subscribe(EventType type, SubscriberCallback callback, void* user_data);

/* 取消订阅 */
void event_bus_unsubscribe(SubscriberHandle handle);

/* 发布事件（线程安全） */
void event_bus_publish(const Event* event);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUS_H */  