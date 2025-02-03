#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>


#include "structures.h"

int current_directory_inode = EXT2_ROOT_INODE;

struct ext2_super_block *superblock;
struct ext2_group_desc *blockgroup;
struct ext2_inode *inodes;
struct ext2_dir_entry *dir;


void printFileType(int filetype) {
    switch (filetype) {
        case EXT2_FT_UNKNOWN:
            printf("Desconhecido\n");
            break;
        case EXT2_FT_REG_FILE:
            printf("Arquivo Regular\n");
            break;
        case EXT2_FT_DIR:
            printf("Diretório\n");
            break;
        case EXT2_FT_CHRDEV:
            printf("Dispositivo de caractere\n");
            break;
        case EXT2_FT_BLKDEV:
            printf("Dispositivo de bloco\n");
            break;
        case EXT2_FT_FIFO:
            printf("FIFO (pipe)\n");
            break;
        case EXT2_FT_SOCK:
            printf("Socket\n");
            break;
        case EXT2_FT_SYMLINK:
            printf("Simbólico\n");
            break;
        default:
            printf("[ERRO] Tipo de Arquivo não encontrado\n");
            break;
    }
}


void print_inode_info(int inode_index) {
    if (!inodes) {
        printf("A tabela de inodes não foi carregada.\n");
        return;
    }

    if (inode_index < 1 || inode_index >= superblock->s_inodes_per_group) {
        printf("[ERRO] Índice de inode inválido: %d\n", inode_index);
        return;
    }

    size_t inode_size = superblock->s_inode_size;
    char *inode_ptr = (char*)inodes + (inode_index - 1) * inode_size;
    struct ext2_inode *inode = (struct ext2_inode*)inode_ptr;

    printf("Inode %d:\n", inode_index);
    printf("  Modo: %o\n", le16toh(inode->i_mode));
    printf("  UID: %u\n", le16toh(inode->i_uid));
    printf("  Tamanho: %u bytes\n", le32toh(inode->i_size));
    printf("  Último acesso: %u\n", le32toh(inode->i_atime));
    printf("  Criação: %u\n", le32toh(inode->i_ctime));
    printf("  Modificação: %u\n", le32toh(inode->i_mtime));
    printf("  Exclusão: %u\n", le32toh(inode->i_dtime));
    printf("  Contagem de links: %u\n", le16toh(inode->i_links_count));
    printf("  Número de blocos: %u\n", le32toh(inode->i_blocks));
    
    printf("  Blocos apontados: ");
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        printf("%u ", le32toh(inode->i_block[i]));
    }
    printf("\n");
}



int findInodeByName(int fd, int base_inode_num, char* filename, int *filetype) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (base_inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *current_group = &blockgroup[group_num];

    off_t inode_table_offset = BLOCK_OFFSET(current_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + ((base_inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return -1;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return -1;
    }

    for (int i = 0; i < EXT2_N_BLOCKS && inode.i_block[i]; i++) {
        offset = BLOCK_OFFSET(inode.i_block[i]);

        while (offset < BLOCK_OFFSET(inode.i_block[i]) + BLOCK_SIZE) {
            if (lseek(fd, offset, SEEK_SET) == -1) {
                perror("[ERRO] Erro ao buscar entrada do diretório");
                return -1;
            }

            bytes_read = read(fd, &entry, sizeof(struct ext2_dir_entry));
            if (bytes_read <= 0) {
                perror("[ERRO] Erro ao ler entrada de diretório");
                return -1;
            }

            if (entry.inode == 0) {
                offset += entry.rec_len;
                continue;
            }

            char name[EXT2_NAME_LEN + 1];
            if (read(fd, name, entry.name_len) != entry.name_len) {
                perror("[ERRO] Erro ao ler nome do arquivo");
                return -1;
            }
            name[entry.name_len] = '\0';

            if (strcmp(name, filename) == 0) {
                *filetype = entry.file_type;
                return entry.inode;
            }

            offset += entry.rec_len;
        }
    }

    return -1; 
}

int statFile(int fd, int base_inode_num, char* filename) {
    int filetype;
    int inode_num = findInodeByName(fd, base_inode_num, filename, &filetype);
    printf("Inode_num stat: %d\n", inode_num);

    if (inode_num == -1) {
        printf("[ERRO] Arquivo/diretório não encontrado: %s\n", filename);
        return -1;
    }

    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *target_group = &blockgroup[group_num];

    off_t inode_table_offset = BLOCK_OFFSET(target_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + 
                        ((inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    struct ext2_inode inode;
    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return -1;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return -1;
    }

    time_t access_time = le32toh(inode.i_atime);
    time_t modify_time = le32toh(inode.i_mtime);
    time_t change_time = le32toh(inode.i_ctime);

    char atime_str[26], mtime_str[26], ctime_str[26];
    strncpy(atime_str, ctime(&access_time), 25);
    strncpy(mtime_str, ctime(&modify_time), 25);
    strncpy(ctime_str, ctime(&change_time), 25);
    atime_str[24] = mtime_str[24] = ctime_str[24] = '\0';

    printf("\n======= Metadados do Arquivo: %s =======\n", filename);
    printFileType(filetype);
    printf("Modo: %o\n", le16toh(inode.i_mode));
    printf("Links: %u\n", le16toh(inode.i_links_count));
    printf("UID: %u\n", le16toh(inode.i_uid));
    printf("GID: %u\n", le16toh(inode.i_gid));
    printf("Tamanho: %u bytes\n", le32toh(inode.i_size));
    printf("Último acesso: %s\n", atime_str);
    printf("Última modificação: %s\n", mtime_str);
    printf("Última alteração nos metadados: %s\n", ctime_str);
    printf("Blocos alocados: %u\n", le32toh(inode.i_blocks));

    printf("Blocos apontados: ");
    for (int j = 0; j < EXT2_N_BLOCKS; j++) {
        printf("%u ", le32toh(inode.i_block[j]));
    }
    printf("\n");

    return 0;
}




void read_superblock(int fd) {
    superblock = malloc(sizeof(struct ext2_super_block));
    if (!superblock) {
        perror("Erro ao alocar memória para superbloco");
        return;
    }

    if (lseek(fd, BASE_OFFSET, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro no superbloco");
        free(superblock);
        return;
    }

    ssize_t bytes_read = read(fd, superblock, sizeof(struct ext2_super_block));
    if (bytes_read != sizeof(struct ext2_super_block)) {
        perror("Erro ao ler superbloco");
        free(superblock);
        return;
    }
}


void read_blockgroup(int fd) {
    int num_groups = (superblock->s_blocks_count - 1) / superblock->s_blocks_per_group + 1;
    size_t group_desc_size = sizeof(struct ext2_group_desc) * num_groups;

    blockgroup = malloc(group_desc_size);
    if (!blockgroup) {
        perror("Erro ao alocar memória para grupos de blocos");
        return;
    }

    off_t blockgroup_offset = BASE_OFFSET + (BLOCK_SIZE << superblock->s_log_block_size);
    if (lseek(fd, blockgroup_offset, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro nos grupos de blocos");
        free(blockgroup);
        return;
    }

    ssize_t bytes_read = read(fd, blockgroup, group_desc_size);
    if (bytes_read != group_desc_size) {
        perror("Erro ao ler grupos de blocos");
        free(blockgroup);
        return;
    }
}


void read_inodeTable(int fd) {
    if (!superblock || !blockgroup) {
        printf("[ERRO] Superblock ou blockgroup não foram carregados.\n");
        return;
    }

    __le32 inodes_per_group = superblock->s_inodes_per_group;
    __le16 inode_size = superblock->s_inode_size;

    size_t table_size = inodes_per_group * inode_size;

    inodes = malloc(table_size);
    if (!inodes) {
        perror("[ERRO] Erro ao alocar memória para a tabela de inodes");
        return;
    }

    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);

    if (lseek(fd, inode_table_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao posicionar ponteiro na tabela de inodes");
        free(inodes);
        return;
    }

    ssize_t bytes_read = read(fd, inodes, table_size);
    if (bytes_read != table_size) {
        perror("[ERRO] Erro ao ler a tabela de inodes");
        free(inodes);
        return;
    }

    printf("Tabela de inodes carregada com sucesso.\n");
}




void ls(int fd, int base_inode_num) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (base_inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *current_group = &blockgroup[group_num];

    off_t inode_table_offset = BLOCK_OFFSET(current_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + ((base_inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return;
    }

    for (int i = 0; i < EXT2_N_BLOCKS && inode.i_block[i]; i++) {
        offset = BLOCK_OFFSET(inode.i_block[i]);

        while (offset < BLOCK_OFFSET(inode.i_block[i]) + BLOCK_SIZE) {

            if (lseek(fd, offset, SEEK_SET) == -1) {
                perror("[ERRO] Erro ao buscar entrada do diretório");
                return;
            }

            bytes_read = read(fd, &entry, sizeof(struct ext2_dir_entry));
            if (bytes_read <= 0) {
                perror("[ERRO] Erro ao ler entrada de diretório");
                return;
            }

            if (entry.inode == 0) {
                offset += entry.rec_len;
                continue;
            }

            char name[EXT2_NAME_LEN + 1];
            if (read(fd, name, entry.name_len) != entry.name_len) {
                perror("[ERRO] Erro ao ler nome do arquivo");
                return;
            }
            name[entry.name_len] = '\0';

            if (entry.file_type == EXT2_FT_DIR) {
                printf("%d\t%s/\n", entry.inode, name);
            } else {
                printf("%d\t%s\n", entry.inode, name);
            }

            offset += entry.rec_len;
        }
    }
}


int cd(int fd, int base_inode_num, char *dirname) {
    if (!inodes) {
        printf("[ERRO] A tabela de inodes não foi carregada.\n");
        return -1;
    }

    if (strcmp(dirname, "..") == 0) {
        if (base_inode_num == EXT2_ROOT_INODE) { 
            printf("[INFO] Já está no diretório raiz.\n");
            return 0;
        }

        struct ext2_inode current_inode;
        __u32 inodes_per_group = superblock->s_inodes_per_group;
        
        int group_num = (base_inode_num - 1) / inodes_per_group;
        struct ext2_group_desc *current_group = &blockgroup[group_num];

        off_t inode_table_offset = BLOCK_OFFSET(current_group->bg_inode_table);
        off_t inode_offset = inode_table_offset + 
                            ((base_inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

        if (lseek(fd, inode_offset, SEEK_SET) == -1) {
            perror("[ERRO] Erro ao buscar inode atual");
            return -1;
        }

        if (read(fd, &current_inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
            perror("[ERRO] Erro ao ler inode atual");
            return -1;
        }

        struct ext2_dir_entry entry;
        char block[BLOCK_SIZE];
        off_t block_offset = BLOCK_OFFSET(current_inode.i_block[0]);

        if (lseek(fd, block_offset, SEEK_SET) == -1) {
            perror("[ERRO] Erro ao buscar bloco do diretório");
            return -1;
        }

        if (read(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("[ERRO] Erro ao ler bloco do diretório");
            return -1;
        }

        struct ext2_dir_entry *dot_entry = (struct ext2_dir_entry *) block;
        struct ext2_dir_entry *dotdot_entry = (struct ext2_dir_entry *) ((char *)dot_entry + le16toh(dot_entry->rec_len));

        current_directory_inode = le32toh(dotdot_entry->inode);
        printf("Voltou um diretório. Novo inode: %d\n", current_directory_inode);
        return 0;
    }

    int filetype;
    int inode_num = findInodeByName(fd, base_inode_num, dirname, &filetype);
    printf("Inode_num cd: %d\n", inode_num);

    if (inode_num == -1) {
        printf("[ERRO] Diretório não encontrado: %s\n", dirname);
        return -1;
    }

    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *target_group = &blockgroup[group_num];

    off_t inode_table_offset = BLOCK_OFFSET(target_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + 
                        ((inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    struct ext2_inode inode;
    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return -1;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return -1;
    }

    if (filetype != EXT2_FT_DIR) {  
        printf("[ERRO] %s não é um diretório.\n", dirname);
        return -1;
    }

    current_directory_inode = inode_num;

    printf("Diretório alterado com sucesso para: %s\n", dirname);
    return 0;
}




void sb() {
    if (!superblock) {
        printf("Superbloco não foi lido ainda. Certifique-se de chamar read_superblock().\n");
        return;
    }
    
    printf("\n========== SUPERBLOCO ==========");
    printf("\nTamanho do sistema de arquivos: %u blocos", superblock->s_blocks_count);
    printf("\nTamanho do bloco: %u bytes", 1024 << superblock->s_log_block_size);
    printf("\nTamanho do inode: %u bytes", superblock->s_inode_size);
    printf("\nNúmero total de inodes: %u", superblock->s_inodes_count);
    printf("\nNúmero de blocos livres: %u", superblock->s_free_blocks_count);
    printf("\nNúmero de inodes livres: %u", superblock->s_free_inodes_count);
    printf("\nPrimeiro inode disponível: %u", superblock->s_first_ino);
    printf("\nVersão do sistema de arquivos: %u", superblock->s_rev_level);
    
    printf("\nUUID do sistema de arquivos: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", superblock->s_uuid[i]);
    }

    time_t mtime = superblock->s_mtime;
    time_t wtime = superblock->s_wtime;
    printf("\nÚltimo tempo de montagem: %s", ctime(&mtime));
    printf("Último tempo de escrita: %s", ctime(&wtime));

    printf("================================\n");
}

void findFile(int fd, int base_inode_num, const char *current_path) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (base_inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *current_group = &blockgroup[group_num];

    off_t inode_table_offset = BLOCK_OFFSET(current_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + 
                        ((base_inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return;
    }

    for (int i = 0; i < EXT2_N_BLOCKS && inode.i_block[i]; i++) {
        offset = BLOCK_OFFSET(inode.i_block[i]);

        while (offset < BLOCK_OFFSET(inode.i_block[i]) + BLOCK_SIZE) {
            if (lseek(fd, offset, SEEK_SET) == -1) {
                perror("[ERRO] Erro ao buscar entrada do diretório");
                return;
            }

            bytes_read = read(fd, &entry, sizeof(struct ext2_dir_entry));
            if (bytes_read <= 0) {
                perror("[ERRO] Erro ao ler entrada de diretório");
                return;
            }

            if (entry.inode == 0) {
                offset += entry.rec_len;
                continue;
            }

            char name[EXT2_NAME_LEN + 1];
            if (read(fd, name, entry.name_len) != entry.name_len) {
                perror("[ERRO] Erro ao ler nome do arquivo");
                return;
            }
            name[entry.name_len] = '\0';

            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                char new_path[1024];
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, name);

                if (entry.file_type == EXT2_FT_DIR) {
                    printf("%s/\n", new_path);
                } else {
                    printf("%s\n", new_path);
                }

                if (entry.file_type == EXT2_FT_DIR) {
                    int subdir_group_num = (entry.inode - 1) / inodes_per_group;
                    struct ext2_group_desc *subdir_group = &blockgroup[subdir_group_num];

                    off_t subdir_inode_table_offset = BLOCK_OFFSET(subdir_group->bg_inode_table);
                    off_t subdir_inode_offset = subdir_inode_table_offset + 
                                              ((entry.inode - 1) % inodes_per_group) * superblock->s_inode_size;

                    struct ext2_inode subdir_inode;
                    if (lseek(fd, subdir_inode_offset, SEEK_SET) == -1) {
                        perror("[ERRO] Erro ao buscar inode do subdiretório");
                        continue;
                    }

                    if (read(fd, &subdir_inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
                        perror("[ERRO] Erro ao ler inode do subdiretório");
                        continue;
                    }

                    findFile(fd, entry.inode, new_path);
                }
            }

            offset += entry.rec_len;
        }
    }
}

int extShell(int fd) {
    char cmd[10];  
    char filename[EXT2_NAME_LEN + 1];
    char target[EXT2_NAME_LEN + 1];
    while (1) {
        printf("ext-shell$ ");
        scanf("%9s", cmd);  

        if (!strcmp(cmd, "q")) {
            return -1;
        } else if (!strcmp(cmd, "sb")) {
            sb();  
        } else if (!strcmp(cmd, "ls")) {
            printf("Listando diretório atual com inode: %d\n", current_directory_inode); 
            ls(fd, current_directory_inode);  
        }
        else if (!strcmp(cmd, "stat")) {
            printf("Digite o nome do arquivo ou diretório: ");
            scanf("%255s", filename); 
            statFile(fd, current_directory_inode, filename);  
        } else if (!strcmp(cmd, "cd")) {
            printf("Digite o nome do diretório: ");
            scanf("%255s", filename);
            cd(fd, current_directory_inode, filename);
        } else if (!strcmp(cmd, "find")) {
            printf("Listando todos os arquivos e diretórios a partir de:\n");
            findFile(fd, current_directory_inode, ".");  
        } else {
            printf("Comando desconhecido: %s\n", cmd);
        }
    }
}



int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage:  ext-shell <file.img>\n");
        return -1;
    }

    int fd = open(argv[1], O_RDONLY | O_SYNC);
    if (fd == -1) {
        printf("Could NOT open file \"%s\"\n", argv[1]);
        return -1;
    }

    read_superblock(fd);
    read_blockgroup(fd);
    read_inodeTable(fd);

    while (1) {
        if (extShell(fd) == -1)
            break;
    }

    printf("\n\nQuitting ext-shell.\n\n");
    return (0);
}