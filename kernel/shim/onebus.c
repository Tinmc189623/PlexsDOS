/*
 * Nexsteaduser — PlexsDOS
 * onebus.c — 消息总线实现 (D-Bus 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 1-Bus: 单机消息队列实现
 *   - 环形缓冲区消息队列
 *   - 基于过滤器的消息派发
 *   - 序列号跟踪
 *   - 无锁 (PlexsDOS 单线程)
 */

#include <plexsdos/onebus.h>
#include <plexsdos/string.h>

/* 内部消息队列 */
static struct onebus_msg msg_queue[ONEBUS_MAX_MSG];
static int queue_head = 0;     /* 读指针 */
static int queue_tail = 0;     /* 写指针 */
static int queue_count = 0;    /* 队列中消息数 */
static uint32_t next_serial = 1;

/* 过滤器链表 */
struct filter_entry {
    onebus_filter_fn callback;
    void *userdata;
    char interface[64];
    struct filter_entry *next;
};

static struct filter_entry *filter_list = NULL;

/*
 * onebus_init — 初始化 1-Bus 系统
 */
bool onebus_init(void)
{
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    next_serial = 1;
    filter_list = NULL;
    return true;
}

/*
 * onebus_shutdown — 关闭 1-Bus 系统
 */
void onebus_shutdown(void)
{
    filter_list = NULL;
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

/*
 * onebus_send — 发送消息
 */
bool onebus_send(const struct onebus_msg *msg)
{
    if (!msg)
        return false;

    if (queue_count >= ONEBUS_MAX_MSG)
        return false;

    /* 复制消息到队列 */
    struct onebus_msg *slot = &msg_queue[queue_tail];
    int i;

    slot->type = msg->type;
    for (i = 0; msg->interface[i] && i < 63; i++)
        slot->interface[i] = msg->interface[i];
    slot->interface[i] = '\0';

    for (i = 0; msg->method[i] && i < 63; i++)
        slot->method[i] = msg->method[i];
    slot->method[i] = '\0';

    for (i = 0; msg->object_path[i] && i < 127; i++)
        slot->object_path[i] = msg->object_path[i];
    slot->object_path[i] = '\0';

    for (i = 0; msg->sender[i] && i < 31; i++)
        slot->sender[i] = msg->sender[i];
    slot->sender[i] = '\0';

    for (i = 0; msg->destination[i] && i < 31; i++)
        slot->destination[i] = msg->destination[i];
    slot->destination[i] = '\0';

    slot->serial = msg->serial;
    slot->reply_serial = msg->reply_serial;
    slot->data_len = msg->data_len;

    if (slot->data_len > ONEBUS_MAX_MSG_SIZE)
        slot->data_len = ONEBUS_MAX_MSG_SIZE;

    for (uint32_t j = 0; j < slot->data_len; j++)
        slot->data[j] = msg->data[j];

    /* 如果发送者未指定, 自动分配序列号 */
    if (slot->serial == 0)
        slot->serial = next_serial++;

    queue_tail = (queue_tail + 1) % ONEBUS_MAX_MSG;
    queue_count++;

    /* 立即派发匹配过滤器的消息 */
    struct filter_entry *f = filter_list;
    while (f) {
        /* 检查接口名是否匹配 */
        int match = 1;
        for (i = 0; f->interface[i] && slot->interface[i]; i++) {
            if (f->interface[i] != slot->interface[i]) {
                match = 0;
                break;
            }
        }
        if (match && f->interface[i] == slot->interface[i])
            f->callback(slot, f->userdata);
        f = f->next;
    }

    return true;
}

/*
 * onebus_recv — 接收消息
 */
bool onebus_recv(struct onebus_msg *msg, int timeout_ms)
{
    (void)timeout_ms;

    if (!msg)
        return false;

    if (queue_count == 0)
        return false;

    /* 复制消息 */
    int i;
    struct onebus_msg *slot = &msg_queue[queue_head];
    msg->type = slot->type;

    for (i = 0; slot->interface[i] && i < 63; i++)
        msg->interface[i] = slot->interface[i];
    msg->interface[i] = '\0';

    for (i = 0; slot->method[i] && i < 63; i++)
        msg->method[i] = slot->method[i];
    msg->method[i] = '\0';

    for (i = 0; slot->object_path[i] && i < 127; i++)
        msg->object_path[i] = slot->object_path[i];
    msg->object_path[i] = '\0';

    for (i = 0; slot->sender[i] && i < 31; i++)
        msg->sender[i] = slot->sender[i];
    msg->sender[i] = '\0';

    for (i = 0; slot->destination[i] && i < 31; i++)
        msg->destination[i] = slot->destination[i];
    msg->destination[i] = '\0';

    msg->serial = slot->serial;
    msg->reply_serial = slot->reply_serial;
    msg->data_len = slot->data_len;

    for (uint32_t j = 0; j < slot->data_len; j++)
        msg->data[j] = slot->data[j];

    queue_head = (queue_head + 1) % ONEBUS_MAX_MSG;
    queue_count--;

    return true;
}

/*
 * onebus_add_filter — 添加消息过滤器
 */
void onebus_add_filter(const char *interface,
                       onebus_filter_fn callback, void *userdata)
{
    if (!interface || !callback)
        return;

    /* 分配新的过滤器条目 (使用静态池, PlexsDOS 无堆) */
    static struct filter_entry filter_pool[8];
    static int pool_used = 0;

    if (pool_used >= 8)
        return;

    struct filter_entry *entry = &filter_pool[pool_used++];
    int i;

    for (i = 0; interface[i] && i < 63; i++)
        entry->interface[i] = interface[i];
    entry->interface[i] = '\0';

    entry->callback = callback;
    entry->userdata = userdata;

    /* 插入链表头 */
    entry->next = filter_list;
    filter_list = entry;
}

/*
 * onebus_remove_filter — 移除消息过滤器
 */
void onebus_remove_filter(onebus_filter_fn callback)
{
    if (!callback)
        return;

    struct filter_entry *prev = NULL;
    struct filter_entry *curr = filter_list;

    while (curr) {
        if (curr->callback == callback) {
            if (prev)
                prev->next = curr->next;
            else
                filter_list = curr->next;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/*
 * onebus_dispatch — 派发所有待处理消息
 */
void onebus_dispatch(void)
{
    /* 消息已在 onebus_send 时同步派发, 无需额外操作 */
}

/*
 * onebus_create_reply — 创建应答消息
 */
void onebus_create_reply(const struct onebus_msg *request,
                         struct onebus_msg *reply,
                         const void *data, uint32_t data_len)
{
    if (!request || !reply)
        return;

    reply->type = ONEBUS_MSG_METHOD_RETURN;
    reply->reply_serial = request->serial;

    int i;
    for (i = 0; request->sender[i] && i < 31; i++)
        reply->destination[i] = request->sender[i];
    reply->destination[i] = '\0';

    for (i = 0; request->interface[i] && i < 63; i++)
        reply->interface[i] = request->interface[i];
    reply->interface[i] = '\0';

    reply->data_len = data_len;
    if (reply->data_len > ONEBUS_MAX_MSG_SIZE)
        reply->data_len = ONEBUS_MAX_MSG_SIZE;

    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t j = 0; j < reply->data_len; j++)
        reply->data[j] = src[j];
}
