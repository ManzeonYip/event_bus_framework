#include "event_bus.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <uthash.h>

/* 订阅者节点结构 */
struct SubscriberNode {
    SubscriberCallback callback;
    void* user_data;
    UT_hash_handle hh;
};

/* 事件类型订阅者映射结构 */
struct EventSubscribers {
    EventType type;
    struct SubscriberNode* subscribers;
    pthread_mutex_t lock;
    UT_hash_handle hh;
};

/* 事件队列节点 */
typedef struct EventNode {
    Event event;
    struct EventNode* next;
} EventNode;

/* 全局状态 */
static struct {
    struct EventSubscribers* subscriptions;
    pthread_mutex_t subscription_lock;
    pthread_t worker_thread;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    EventNode* queue_head;
    EventNode* queue_tail;
    int running;
} g_event_bus = {0};

/* 工作线程函数 */
static void* event_worker(void* arg) {
    while (g_event_bus.running) {
        EventNode* node = NULL;
        
        /* 等待事件队列有数据 */
        pthread_mutex_lock(&g_event_bus.queue_lock);
        while (g_event_bus.running && !g_event_bus.queue_head) {
            pthread_cond_wait(&g_event_bus.queue_cond, &g_event_bus.queue_lock);
        }
        
        if (!g_event_bus.running) {
            pthread_mutex_unlock(&g_event_bus.queue_lock);
            break;
        }
        
        /* 从队列取出事件 */
        node = g_event_bus.queue_head;
        g_event_bus.queue_head = node->next;
        if (!g_event_bus.queue_head) {
            g_event_bus.queue_tail = NULL;
        }
        pthread_mutex_unlock(&g_event_bus.queue_lock);
        
        /* 查找订阅者并分发事件 */
        pthread_mutex_lock(&g_event_bus.subscription_lock);
        struct EventSubscribers* subs_map = NULL;
        HASH_FIND(hh, g_event_bus.subscriptions, &node->event.type, sizeof(EventType), subs_map);
        pthread_mutex_unlock(&g_event_bus.subscription_lock);
        
        if (subs_map) {
            struct SubscriberNode* current_sub;
            pthread_mutex_lock(&subs_map->lock);
            HASH_ITER(hh, subs_map->subscribers, current_sub, tmp) {
                current_sub->callback(&node->event, current_sub->user_data);
            }
            pthread_mutex_unlock(&subs_map->lock);
        }
        
        /* 释放事件内存 */
        if (node->event.data) {
            free(node->event.data);
        }
        free(node);
    }
    return NULL;
}

void event_bus_init(void) {
    pthread_mutex_init(&g_event_bus.subscription_lock, NULL);
    pthread_mutex_init(&g_event_bus.queue_lock, NULL);
    pthread_cond_init(&g_event_bus.queue_cond, NULL);
    g_event_bus.subscriptions = NULL;
    g_event_bus.queue_head = NULL;
    g_event_bus.queue_tail = NULL;
    g_event_bus.running = 1;
    
    /* 创建工作线程 */
    pthread_create(&g_event_bus.worker_thread, NULL, event_worker, NULL);
}

void event_bus_destroy(void) {
    /* 停止工作线程 */
    pthread_mutex_lock(&g_event_bus.queue_lock);
    g_event_bus.running = 0;
    pthread_cond_signal(&g_event_bus.queue_cond);
    pthread_mutex_unlock(&g_event_bus.queue_lock);
    
    pthread_join(&g_event_bus.worker_thread, NULL);
    
    /* 清理订阅者 */
    struct EventSubscribers* subs_map, *tmp_subs;
    HASH_ITER(hh, g_event_bus.subscriptions, subs_map, tmp_subs) {
        struct SubscriberNode* sub, *tmp_sub;
        HASH_ITER(hh, subs_map->subscribers, sub, tmp_sub) {
            HASH_DEL(subs_map->subscribers, sub);
            free(sub);
        }
        pthread_mutex_destroy(&subs_map->lock);
        HASH_DEL(g_event_bus.subscriptions, subs_map);
        free(subs_map);
    }
    
    /* 清理事件队列 */
    EventNode* node = g_event_bus.queue_head;
    while (node) {
        EventNode* next = node->next;
        if (node->event.data) {
            free(node->event.data);
        }
        free(node);
        node = next;
    }
    
    /* 销毁锁和条件变量 */
    pthread_mutex_destroy(&g_event_bus.subscription_lock);
    pthread_mutex_destroy(&g_event_bus.queue_lock);
    pthread_cond_destroy(&g_event_bus.queue_cond);
}

SubscriberHandle event_bus_subscribe(EventType type, SubscriberCallback callback, void* user_data) {
    if (!callback) return NULL;
    
    struct SubscriberNode* new_sub = malloc(sizeof(struct SubscriberNode));
    if (!new_sub) return NULL;
    
    new_sub->callback = callback;
    new_sub->user_data = user_data;
    
    pthread_mutex_lock(&g_event_bus.subscription_lock);
    
    /* 查找或创建事件类型的订阅者映射 */
    struct EventSubscribers* subs_map = NULL;
    HASH_FIND(hh, g_event_bus.subscriptions, &type, sizeof(EventType), subs_map);
    
    if (!subs_map) {
        subs_map = malloc(sizeof(struct EventSubscribers));
        if (!subs_map) {
            free(new_sub);
            pthread_mutex_unlock(&g_event_bus.subscription_lock);
            return NULL;
        }
        subs_map->type = type;
        subs_map->subscribers = NULL;
        pthread_mutex_init(&subs_map->lock, NULL);
        HASH_ADD(hh, g_event_bus.subscriptions, type, sizeof(EventType), subs_map);
    }
    
    /* 添加新订阅者 */
    pthread_mutex_lock(&subs_map->lock);
    HASH_ADD(hh, subs_map->subscribers, callback, sizeof(SubscriberCallback), new_sub);
    pthread_mutex_unlock(&subs_map->lock);
    
    pthread_mutex_unlock(&g_event_bus.subscription_lock);
    
    return (SubscriberHandle)new_sub;
}

void event_bus_unsubscribe(SubscriberHandle handle) {
    if (!handle) return;
    
    struct SubscriberNode* sub = (struct SubscriberNode*)handle;
    
    pthread_mutex_lock(&g_event_bus.subscription_lock);
    
    /* 查找事件类型的订阅者映射 */
    struct EventSubscribers* subs_map = NULL;
    HASH_FIND(hh, g_event_bus.subscriptions, &sub->type, sizeof(EventType), subs_map);
    
    if (subs_map) {
        pthread_mutex_lock(&subs_map->lock);
        HASH_DEL(subs_map->subscribers, sub);
        pthread_mutex_unlock(&subs_map->lock);
        free(sub);
    }
    
    pthread_mutex_unlock(&g_event_bus.subscription_lock);
}

void event_bus_publish(const Event* event) {
    if (!event) return;
    
    /* 创建事件副本 */
    EventNode* node = malloc(sizeof(EventNode));
    if (!node) return;
    
    node->event.type = event->type;
    node->event.size = event->size;
    node->event.data = NULL;
    
    /* 复制事件数据 */
    if (event->data && event->size > 0) {
        node->event.data = malloc(event->size);
        if (!node->event.data) {
            free(node);
            return;
        }
        memcpy(node->event.data, event->data, event->size);
    }
    
    node->next = NULL;
    
    /* 将事件添加到队列 */
    pthread_mutex_lock(&g_event_bus.queue_lock);
    
    if (!g_event_bus.queue_tail) {
        g_event_bus.queue_head = g_event_bus.queue_tail = node;
    } else {
        g_event_bus.queue_tail->next = node;
        g_event_bus.queue_tail = node;
    }
    
    /* 通知工作线程 */
    pthread_cond_signal(&g_event_bus.queue_cond);
    pthread_mutex_unlock(&g_event_bus.queue_lock);
}  