#ifndef __READDIR_H__
#define __READDIR_H__


DIR *opendir(const char *name);
dirent64_t *readdir(DIR *dir);



#endif
