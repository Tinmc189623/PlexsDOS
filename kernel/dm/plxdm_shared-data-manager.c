/*
 * Nexsteaduser — PlexsDOS
 * plxdm_shared-data-manager.c — 共享数据管理器 (PlexsDOS 原生实现)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * PlexsDOS 无完整文件系统, 共享数据目录操作为空操作。
 */

#include "plxdm_shared-data-manager.h"

/* USERS_DIR: PlexsDOS 用户数据目录 */
#define USERS_DIR "/system/users"

/* 私有数据结构 (当前无文件系统操作, 为空) */
typedef struct { int dummy; } SharedDataManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SharedDataManager, shared_data_manager, G_TYPE_OBJECT)

static SharedDataManager *singleton = NULL;

SharedDataManager *
shared_data_manager_get_instance (void)
{
    if (!singleton)
        singleton = g_object_new (SHARED_DATA_MANAGER_TYPE, NULL);
    return singleton;
}

void
shared_data_manager_cleanup (void)
{
    /* PlexsDOS: 无文件系统操作 */
}

void
shared_data_manager_start (SharedDataManager *manager)
{
    /* PlexsDOS: 无文件系统操作 */
    (void)manager;
}

gchar *
shared_data_manager_ensure_user_dir (SharedDataManager *manager, const gchar *user)
{
    /* PlexsDOS: 返回固定路径 */
    (void)manager;
    return g_strdup_printf ("%s/%s", USERS_DIR, user);
}

static void
shared_data_manager_init (SharedDataManager *manager)
{
    (void)manager;
}

static void
shared_data_manager_finalize (GObject *object)
{
    /* PlexsDOS: 无 parent class chain-up */
}

static void
shared_data_manager_class_init (SharedDataManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = shared_data_manager_finalize;
}

PLXDM_DEFINE_TYPE_GET_TYPE (SharedDataManager, shared_data_manager, G_TYPE_OBJECT)
