#include "event_bus.h"
#include <stdio.h>

/* 自定义事件数据结构 */
typedef struct {
    int value;
    const char* message;
} CustomEventData;

/* 订阅者回调函数 */
static void subscriber_callback(const Event* event, void* user_data) {
    printf("收到事件类型: %u\n", event->type);

    if (event->data && event->size >= sizeof(CustomEventData)) {
        const CustomEventData* data = (const CustomEventData*)event->data;
        printf("  值: %d\n", data->value);
        printf("  消息: %s\n", data->message);
    }

    if (user_data) {
        printf("  用户数据: %s\n", (const char*)user_data);
    }
}

int main() {
    /* 初始化事件总线 */
    event_bus_init();

    /* 定义事件类型 */
    enum {
        EVENT_TYPE_A = 1,
        EVENT_TYPE_B = 2
    };

    /* 订阅事件 */
    SubscriberHandle handle1 = event_bus_subscribe(EVENT_TYPE_A, subscriber_callback, "订阅者1");
    SubscriberHandle handle2 = event_bus_subscribe(EVENT_TYPE_A, subscriber_callback, "订阅者2");
    SubscriberHandle handle3 = event_bus_subscribe(EVENT_TYPE_B, subscriber_callback, "订阅者3");

    /* 准备事件数据 */
    CustomEventData event_data = {
        .value = 42,
        .message = "你好，事件总线！"
    };

    Event event = {
        .type = EVENT_TYPE_A,
        .size = sizeof(event_data),
        .data = &event_data
    };

    /* 发布事件 */
    event_bus_publish(&event);

    /* 修改事件数据并发布另一种类型的事件 */
    event_data.value = 99;
    event_data.message = "另一个事件";
    event.type = EVENT_TYPE_B;

    event_bus_publish(&event);

    /* 取消订阅 */
    event_bus_unsubscribe(handle1);
    event_bus_unsubscribe(handle2);
    event_bus_unsubscribe(handle3);

    /* 销毁事件总线 */
    event_bus_destroy();

    return 0;
}
