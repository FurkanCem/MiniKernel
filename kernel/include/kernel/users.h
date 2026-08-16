#ifndef KERNEL_USERS_H
#define KERNEL_USERS_H
#define USER_NAME_MAX 20
#define USER_MAX_ACCOUNTS 16

void users_init(void);

int users_authenticate(const char *username, const char *password,
                       unsigned int *out_uid);

int users_add(const char *username, const char *password, unsigned int uid);

int users_name_for_uid(unsigned int uid, char *out, unsigned int out_len);

#endif
