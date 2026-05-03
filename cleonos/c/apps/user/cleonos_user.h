#ifndef CLEONOS_USER_H
#define CLEONOS_USER_H

#include <cleonos_syscall.h>

#define CLEONOS_USER_HASH_HEX_LEN 64U
#define CLEONOS_USER_RECORD_MAX 192U
#define CLEONOS_USER_DB_PATH "/system/users.db"
#define CLEONOS_USER_SESSION_PATH "/temp/.cleonos_user_session"

typedef struct cleonos_user_record {
    char name[CLEONOS_USER_NAME_MAX];
    char hash[CLEONOS_USER_HASH_HEX_LEN + 1U];
    u64 role;
    char home[CLEONOS_USER_HOME_MAX];
} cleonos_user_record;

int cleonos_user_is_disk_boot(void);
int cleonos_user_name_valid(const char *name);
void cleonos_user_home_for(char *out_home, u64 out_size, const char *name);
void cleonos_user_hash_password(const char *password, char out_hex[CLEONOS_USER_HASH_HEX_LEN + 1U]);
int cleonos_user_db_exists(void);
int cleonos_user_find(const char *name, cleonos_user_record *out_record);
int cleonos_user_verify_password(const char *name, const char *password, cleonos_user_record *out_record);
int cleonos_user_create(const char *name, const char *password, u64 role, int allow_existing);
int cleonos_user_change_password(const char *name, const char *new_password);
int cleonos_user_set_role(const char *name, u64 role);
int cleonos_user_remove(const char *name);
int cleonos_user_list(void (*emit)(const cleonos_user_record *record, void *ctx), void *ctx);
int cleonos_user_session_write(const cleonos_user_record *record);
int cleonos_user_session_read(cleonos_user_record *out_record);
int cleonos_user_session_clear(void);
int cleonos_user_current_is_admin(void);

#endif
