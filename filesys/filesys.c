#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/inode.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/fat.h"
#include "include/threads/thread.h"


/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
	//루트 디렉토리 설정
	thread_current()->cur_dir = dir_open_root(); //dir_open_root: root디렉토리 정보 반환
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	bool success = false;
#ifdef EFILESYS
    // name의 파일경로를 cp_name에 복사
    char *cp_name = (char *)malloc(strlen(name) + 1);
    strlcpy(cp_name, name, strlen(name) + 1);

    // cp_name의 경로분석
    char *file_name = (char *)malloc(strlen(name) + 1);
    struct dir *dir = parse_path(cp_name, file_name);

    cluster_t inode_cluster = fat_create_chain(0);

    success = (dir != NULL
               // 파일의 inode를 생성하고 디렉토리에 추가한다
               && inode_create(inode_cluster, initial_size, 0)
               && dir_add(dir, file_name, inode_cluster));

    if (!success && inode_cluster != 0) {
        fat_remove_chain(inode_cluster, 0);
    }

    dir_close(dir);
    free(cp_name);
    free(file_name);
    return success;

#else
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size,0)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir);

	return success;
#endif
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
#ifdef EFILESYS
    // name의 파일경로를 cp_name에 복사
    char* cp_name = (char *)malloc(strlen(name) + 1);
    char* file_name = (char *)malloc(strlen(name) + 1);

    struct dir* dir = NULL;
    struct inode *inode = NULL;

    while(true) {
        strlcpy(cp_name, name, strlen(name) + 1);
        // cp_name의경로분석
        dir = parse_path(cp_name, file_name);

        if (dir != NULL) {
            dir_lookup(dir, file_name, &inode);
            if(inode && inode->data.is_link) {   // 파일이 존재하고, 링크 파일인 경우
                dir_close(dir);
                name = inode->data.link_name;
            }
        }
        free(cp_name);
        free(file_name);
        dir_close(dir);
        break;
    }
    return file_open(inode);
#else
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode);
	dir_close (dir);

	return file_open (inode);
#endif
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
    #ifdef EFILESYS

    // name의 파일경로를 cp_name에 복사
    char* cp_name = (char *)malloc(strlen(name) + 1);
    strlcpy(cp_name, name, strlen(name) + 1);

    // cp_name의 경로분석
    char* file_name = (char *)malloc(strlen(name) + 1);
    struct dir *dir = parse_path(cp_name, file_name);

    struct inode *inode = NULL;
    bool success = false;

    if (dir != NULL) {
        dir_lookup(dir, file_name, &inode);

        if(inode_is_dir(inode)) {   // 디렉토리인 경우
            struct dir* cur_dir = dir_open(inode);
            char* tmp = (char *)malloc(NAME_MAX + 1);
            dir_seek(cur_dir, 2 * sizeof(struct dir_entry));

            if(!dir_readdir(cur_dir, tmp)) {   // 디렉토리가 비었다
                // 현재 디렉토리가 아니면 지우게 한다
                if(inode_get_inumber(dir_get_inode(thread_current()->cur_dir)) != inode_get_inumber(dir_get_inode(cur_dir)))
                    success = dir_remove(dir, file_name);
            }

            else {   // 디렉토리가 비지 않았다.
                // 찾은 디렉토리에서 지운다
                success = dir_remove(cur_dir, file_name);
            }
            
            dir_close(cur_dir);
            free(tmp);
        }
        else {   // 파일인 경우
            inode_close(inode);
            success = dir_remove(dir, file_name);
        }
    }

    dir_close(dir);
    free(cp_name);
    free(file_name);

    return success;

    #else

	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;

    #endif
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();

	//'.','..' 파일 추가
	if(!dir_create(ROOT_DIR_CLUSTER,16))
		PANIC("root directory creation failed");

	struct dir* root_dir = dir_open_root();
	dir_add(root_dir, ".", ROOT_DIR_SECTOR);
	dir_add(root_dir, "..", ROOT_DIR_SECTOR);
	dir_close(root_dir);

	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

//디렉토리 생성하기 위한 함수 구현
bool filesys_create_dir(const char* name){
	bool success = false;
	
	//name의 파일경로를 cp_name에 복사
	char *cp_name = (char *)malloc(strlen(name)+1);
	strlcpy(cp_name, name, strlen(name)+1);

	//name경로분석
	char* file_name = (char*)malloc(strlen(name)+1);
	struct dir *dir = parse_path(cp_name,file_name);

	//bitmap에서 inode sector 번호 할당
	cluster_t inode_cluster = fat_create_chain(0);
	struct inode *sub_dir_inode;
	struct dir *sub_dir = NULL;

	/*할당받은 sector에 file_name의 디렉토리 생성
	디렉토리 엔트리에 file_name의 엔트리 추가
	디렉토리 엔트리에 ".", ".." 파일의 엔트리 추가*/
	success = (
		dir != NULL
		&& dir_create(inode_cluster,16)
		&& dir_add(dir,file_name,inode_cluster)
		&& dir_lookup(dir,file_name,&sub_dir_inode)
		&& dir_add(sub_dir= dir_open(sub_dir_inode), ".", inode_cluster)
		&& dir_add(sub_dir,"..", inode_get_inumber(dir_get_inode(dir)))
	);

	if(!success && inode_cluster != 0){
		fat_remove_chain(inode_cluster,0);
	}
	dir_close(sub_dir);
	dir_close(dir);

	free(cp_name);
	free(file_name);
	return success;
}

//경로분석함수 구현
// file_name: path_name을 분석하여 파일, 디렉터리의 이름을 포인팅
struct dir *parse_path(char *path_name, char* file_name){
	struct dir *dir = NULL;
	if(path_name == NULL || file_name == NULL)
		return NULL;
	if(strlen(path_name) == 0)
		return NULL;
	
	//path_name의 절대/상대 경로에 따른 디렉터리 정보 저장
	if(path_name[0] == '/'){
		dir = dir_open_root();
	}
	else{
		dir = dir_reopen(thread_current()->cur_dir);
	}

	char *token, *nextToken, *savePtr;
	token = strtok_r(path_name, "/", &savePtr);
	nextToken = strtok_r(NULL, "/", &savePtr);

	// '/'를 open하려는 케이스
	if(token == NULL){
		token = (char *)malloc(2);
		strlcpy(token,".",2);
	}

	struct inode *inode;

	while(token != NULL && nextToken != NULL){
		//dir에서 token이름의 파일을 검색해 inode의 정보를 저장
		if(!dir_lookup(dir,token, &inode)){
			dir_close(dir);
			return NULL;
		}
		if(inode->data.is_link){ //링크파일인 경우
            char* new_path = (char*)malloc(sizeof(strlen(inode->data.link_name)) + 1);
            strlcpy(new_path, inode->data.link_name, strlen(inode->data.link_name) + 1);

            strlcpy(path_name, new_path, strlen(new_path) + 1);
            free(new_path);

			strlcat(path_name, "/", strlen(path_name)+2);
			strlcat(path_name, nextToken, strlen(path_name)+strlen(nextToken)+1);
			strlcat(path_name, savePtr, strlen(path_name)+strlen(savePtr)+1);

			dir_close(dir);

			//파싱된 경로로 다시 시작함
			if(path_name[0] == '/'){
				dir = dir_open_root();
			}
			else{
				dir = dir_reopen(thread_current()->cur_dir);
			}

			token = strtok_r(path_name, "/", &savePtr);
			nextToken = strtok_r(NULL, "/", &savePtr);

			continue;
		}

		//inode가 파일일 경우 NULL반환
		if(!inode_is_dir(inode)){
			dir_close(dir);
			inode_close(inode);
			return NULL;
		}
		//dir의 디렉토리 정보를 메모리에서 해제
		dir_close(dir);

		//inode의 디렉토리 정보를 dir에 저장
		dir=dir_open(inode);

		//token에 검색할 경로이름 저장
		token = nextToken;
		nextToken = strtok_r(NULL,"/",&savePtr);
	}
	//token의 파일이름을 file_name에 저장
	strlcpy(file_name, token, strlen(token)+1);

	//dir정보반환
	return dir;
}