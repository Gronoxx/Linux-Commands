// struct inodes deve armazenar toda a tabela de inodes, dai vc utiliza isso para impriomir no stats
//


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

    // Verifica se o índice do inode é válido
    if (inode_index < 1 || inode_index >= superblock->s_inodes_per_group) {
        printf("[ERRO] Índice de inode inválido: %d\n", inode_index);
        return;
    }

    // Calcula o deslocamento do inode na tabela
    size_t inode_size = superblock->s_inode_size;
    char *inode_ptr = (char*)inodes + (inode_index - 1) * inode_size;
    struct ext2_inode *inode = (struct ext2_inode*)inode_ptr;

    // Converte campos para a endianess do host (se necessário)
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

    // Passo 1: Encontrar o grupo de blocos do inode base
    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (base_inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *current_group = &blockgroup[group_num];

    // Passo 2: Calcular o deslocamento do inode base
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

    return -1; // Arquivo não encontrado
}



/*
int statFile(int fd, int base_inode_num, char* filename) {
    int filetype;
    int inode_num = findInodeByName(fd, base_inode_num, filename, &filetype);

    if (inode_num == -1) {
        printf("[ERRO] Arquivo ou diretório não encontrado: %s\n", filename);
        return -1;
    }

    // Localizando o inode para o arquivo/diretório encontrado
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);
    off_t inode_offset = inode_table_offset + (inode_num - 1) * sizeof(struct ext2_inode);

    struct ext2_inode *inode;
    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return -1;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return -1;
    }

    // Convertendo tempos para formato legível
    time_t ctime = inode->i_ctime;
    time_t atime = inode->i_atime;
    time_t mtime = inode->i_mtime;

    char ctime_str[26], atime_str[26], mtime_str[26];
    ctime_r(&ctime, ctime_str);  // Converte tempo de criação
    ctime_r(&atime, atime_str);  // Converte tempo de último acesso
    ctime_r(&mtime, mtime_str);  // Converte tempo de última modificação

    // Exibindo os metadados do arquivo/diretório
    printf("Metadados do arquivo/diretório: %s\n", filename);
    printf("Inode: %d\n", inode_num);
    printf("Tamanho: %u bytes\n", inode->i_size);
    printf("Blocos: %u\n", inode->i_blocks);
    printf("Links: %u\n", inode->i_links_count);
    printf("UID do proprietário: %u\n", inode->i_uid);
    printf("GID do grupo: %u\n", inode->i_gid);
    printf("Tempo de criação: %s", ctime_str);  // Inclui '\n' ao final
    printf("Último acesso: %s", atime_str);     // Inclui '\n' ao final
    printf("Última modificação: %s", mtime_str); // Inclui '\n' ao final
    printf("Modo: 0x%x\n", inode->i_mode);

    // Verificando se é diretório ou arquivo regular
    if (inode->i_mode & 0x4000) {
        printf("Tipo: Diretório\n");
    } else if (inode->i_mode & 0x8000) {
        printf("Tipo: Arquivo regular\n");
    } else {
        printf("Tipo: Outro\n");
    }

    return 0;  // Sucesso
}
*/

int statFile(int fd, int base_inode_num, char* filename) {
    if (!inodes) {
        printf("[ERRO] A tabela de inodes não foi carregada.\n");
        return -1;
    }

    int filetype;
    int inode_num = findInodeByName(fd, base_inode_num, filename, &filetype);
    printf("Inode_num stat: %d\n", inode_num); // Adicionei \n para clareza

    if (inode_num == -1) {
        printf("[ERRO] Arquivo ou diretório não encontrado: %s\n", filename);
        return -1;
    }

    // Verificação corrigida: inodes são indexados a partir de 1
    if (inode_num < 1 || inode_num >= superblock->s_inodes_per_group) {
        printf("[ERRO] Número de inode inválido (%d).\n", inode_num);
        return -1;
    }
    
    size_t inode_size = superblock->s_inode_size;
    char *inode_ptr = (char*)inodes + (inode_num - 1) * inode_size;
    struct ext2_inode *inode = (struct ext2_inode*)inode_ptr;


    // Convertendo campos para a endianess do host
    time_t access_time = le32toh(inode->i_atime); // Little-endian para host
    time_t modify_time = le32toh(inode->i_mtime);
    time_t change_time = le32toh(inode->i_ctime);

    // Convertendo timestamps
    char atime_str[26], mtime_str[26], ctime_str[26];
    strncpy(atime_str, ctime(&access_time), 25);
    strncpy(mtime_str, ctime(&modify_time), 25);
    strncpy(ctime_str, ctime(&change_time), 25);
    atime_str[24] = mtime_str[24] = ctime_str[24] = '\0'; // Remove a quebra de linha

    printf("\n======= Metadados do Arquivo: %s =======\n", filename);
    printFileType(filetype);
    printf("Modo: %o\n", le16toh(inode->i_mode)); // Convertendo modo
    printf("Número de links: %u\n", le16toh(inode->i_links_count));
    printf("UID do dono: %u\n", le16toh(inode->i_uid));
    printf("GID do grupo: %u\n", le16toh(inode->i_gid));
    printf("Tamanho: %u bytes\n", le32toh(inode->i_size)); // Convertendo tamanho
    printf("Último acesso: %s\n", atime_str);
    printf("Última modificação: %s\n", mtime_str);
    printf("Última alteração nos metadados: %s\n", ctime_str);
    printf("Blocos alocados: %u\n", le32toh(inode->i_blocks)); // Convertendo blocos

    printf("Blocos apontados: ");
    for (int j = 0; j < EXT2_N_BLOCKS; j++) {
        printf("%u ", le32toh(inode->i_block[j])); // Convertendo blocos
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

    // O superbloco começa no deslocamento 1024 bytes (BASE_OFFSET)
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

    // O descritor dos grupos começa no bloco 2 se o tamanho do bloco for 1024
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
    // Verifica se o superbloco e o grupo de blocos foram carregados
    if (!superblock || !blockgroup) {
        printf("[ERRO] Superblock ou blockgroup não foram carregados.\n");
        return;
    }

    // Obtém o número de inodes por grupo e o tamanho de cada inode
    __le32 inodes_per_group = superblock->s_inodes_per_group;
    __le16 inode_size = superblock->s_inode_size;

    // Calcula o tamanho total da tabela de inodes
    size_t table_size = inodes_per_group * inode_size;

    // Aloca memória para a tabela de inodes
    inodes = malloc(table_size);
    if (!inodes) {
        perror("[ERRO] Erro ao alocar memória para a tabela de inodes");
        return;
    }

    // Calcula o deslocamento da tabela de inodes
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);

    // Posiciona o ponteiro do arquivo no início da tabela de inodes
    if (lseek(fd, inode_table_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao posicionar ponteiro na tabela de inodes");
        free(inodes);
        return;
    }

    // Lê a tabela de inodes do disco
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

    // Passo 1: Encontrar o grupo de blocos do inode atual
    __u32 inodes_per_group = superblock->s_inodes_per_group;
    int group_num = (base_inode_num - 1) / inodes_per_group;
    struct ext2_group_desc *current_group = &blockgroup[group_num];

    // Passo 2: Calcular o deslocamento da tabela de inodes do grupo correto
    off_t inode_table_offset = BLOCK_OFFSET(current_group->bg_inode_table);
    off_t inode_offset = inode_table_offset + ((base_inode_num - 1) % inodes_per_group) * superblock->s_inode_size;

    // Posiciona o ponteiro para o inode desejado
    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return;
    }

    // Lê o inode do diretório base
    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return;
    }

    // Percorre os blocos de dados do diretório
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

            // Nome do arquivo
            char name[EXT2_NAME_LEN + 1];
            if (read(fd, name, entry.name_len) != entry.name_len) {
                perror("[ERRO] Erro ao ler nome do arquivo");
                return;
            }
            name[entry.name_len] = '\0';

            // Verifica se a entrada é um diretório
            if (entry.file_type == EXT2_FT_DIR) {
                printf("%d\t%s/\n", entry.inode, name);
            } else {
                printf("%d\t%s\n", entry.inode, name);
            }

            // Avança para a próxima entrada
            offset += entry.rec_len;
        }
    }
}


int cd(int fd, int base_inode_num, char *dirname) {
    if (!inodes) {
        printf("[ERRO] A tabela de inodes não foi carregada.\n");
        return -1;
    }

    // Se o usuário quiser voltar um diretório
    if (strcmp(dirname, "..") == 0) {
        if (base_inode_num == EXT2_ROOT_INODE) { 
            printf("[INFO] Já está no diretório raiz.\n");
            return 0; // Já no root, não pode subir mais
        }

        // Buscar o inode do diretório pai (sempre o segundo entry em um diretório)
        struct ext2_dir_entry *entry;
        struct ext2_inode *base_inode = &inodes[base_inode_num - 1];

        if (!(base_inode->i_mode & 0x4000)) { 
            printf("[ERRO] O inode base não é um diretório.\n");
            return -1;
        }

        // Ler os dados do diretório atual
        char block[BLOCK_SIZE];
        lseek(fd, BLOCK_OFFSET(base_inode->i_block[0]), SEEK_SET);
        read(fd, block, BLOCK_SIZE);

        entry = (struct ext2_dir_entry *) block; // Primeiro entry é ".", segundo é ".."
        entry = (struct ext2_dir_entry *) ((char *)entry + entry->rec_len);

        // O inode do diretório pai
        current_directory_inode = entry->inode;
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

    // Verificar se o inode encontrado é de um diretório
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);
    off_t inode_offset = inode_table_offset + (inode_num - 1) * sizeof(struct ext2_inode);

    struct ext2_inode inode;
    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return -1;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return -1;
    }

    // Verificar se o inode representa um diretório
    if (filetype != EXT2_FT_DIR) {  
        printf("[ERRO] %s não é um diretório.\n", dirname);
        return -1;
    }

    // Atualizar o inode do diretório atual
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

    // Conversão de timestamps
    time_t mtime = superblock->s_mtime;
    time_t wtime = superblock->s_wtime;
    printf("\nÚltimo tempo de montagem: %s", ctime(&mtime));
    printf("Último tempo de escrita: %s", ctime(&wtime));

    printf("================================\n");
}

// Protótipo da função
void findFile(int fd, int base_inode_num, const char *current_path) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    // Lê o inode do diretório atual
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);
    off_t inode_offset = inode_table_offset + (base_inode_num - 1) * sizeof(struct ext2_inode);

    if (lseek(fd, inode_offset, SEEK_SET) == -1) {
        perror("[ERRO] Erro ao buscar inode");
        return;
    }

    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("[ERRO] Erro ao ler inode");
        return;
    }

    // Percorre os blocos do diretório
    for (int i = 0; i < EXT2_N_BLOCKS && inode.i_block[i]; i++) {
        offset = BLOCK_OFFSET(inode.i_block[i]);

        while (offset < BLOCK_OFFSET(inode.i_block[i]) + BLOCK_SIZE) {
            if (lseek(fd, offset, SEEK_SET) == -1) {
                perror("[ERRO] Erro ao buscar entrada do diretório");
                return;
            }

            // Lê a entrada do diretório
            bytes_read = read(fd, &entry, sizeof(struct ext2_dir_entry));
            if (bytes_read <= 0) {
                perror("[ERRO] Erro ao ler entrada de diretório");
                return;
            }

            if (entry.inode == 0) {
                offset += entry.rec_len;
                continue;
            }

            // Lê o nome do arquivo/diretório
            char name[EXT2_NAME_LEN + 1];
            if (read(fd, name, entry.name_len) != entry.name_len) {
                perror("[ERRO] Erro ao ler nome do arquivo");
                return;
            }
            name[entry.name_len] = '\0';

            // Ignora '.' e '..'
            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                // Monta o caminho completo
                char new_path[1024];
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, name);

                // Exibe o caminho (adiciona '/' para diretórios)
                if (entry.file_type == EXT2_FT_DIR) {
                    printf("%s/\n", new_path);
                } else {
                    printf("%s\n", new_path);
                }

                // Se for diretório, busca recursivamente
                if (entry.file_type == EXT2_FT_DIR) {
                    struct ext2_inode subdir_inode;
                    off_t subdir_inode_offset = inode_table_offset + (entry.inode - 1) * sizeof(struct ext2_inode);

                    if (lseek(fd, subdir_inode_offset, SEEK_SET) == -1) {
                        perror("[ERRO] Erro ao buscar inode do subdiretório");
                        continue;
                    }

                    if (read(fd, &subdir_inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
                        perror("[ERRO] Erro ao ler inode do subdiretório");
                        continue;
                    }

                    // Chama recursivamente para o subdiretório
                    findFile(fd, entry.inode, new_path);
                }
            }

            offset += entry.rec_len;
        }
    }
}

int extShell(int fd) {
    char cmd[10];  // Suporta comandos de até 9 letras + '\0'
    char filename[EXT2_NAME_LEN + 1];
    char target[EXT2_NAME_LEN + 1];
    while (1) {
        printf("ext-shell$ ");
        scanf("%9s", cmd);  // Limita entrada a 9 caracteres para evitar buffer overflow

        if (!strcmp(cmd, "q")) {
            return -1;
        } else if (!strcmp(cmd, "sb")) {
            sb();  // Exibe informações do super bloco
        } else if (!strcmp(cmd, "ls")) {
            printf("Listando diretório atual com inode: %d\n", current_directory_inode);  // Depuração: Mostra o inode atual
            ls(fd, current_directory_inode);  // Lista arquivos a partir do diretório atual
        }
  // Lista arquivos a partir do diretório raiz
        else if (!strcmp(cmd, "stat")) {
            // Solicita o nome do arquivo/diretório a ser examinado
            printf("Digite o nome do arquivo ou diretório: ");
            scanf("%255s", filename);  // Lê o nome do arquivo (máx 255 caracteres)
            statFile(fd, EXT2_ROOT_INODE, filename);  // Chama a função statFile
        } else if (!strcmp(cmd, "cd")) {
            printf("Digite o nome do diretório: ");
            scanf("%255s", filename);
            cd(fd, current_directory_inode, filename);
        } else if (!strcmp(cmd, "find")) {
            printf("Listando todos os arquivos e diretórios a partir de:\n");
            findFile(fd, current_directory_inode, ".");  // Inicia no diretório atual
        } else {
            printf("Comando desconhecido: %s\n", cmd);
        }
    }
}



int main(int argc, char **argv)
{
    // open up the disk file
    if (argc != 2) {
        printf("usage:  ext-shell <file.img>\n");
        return -1;
    }

    int fd = open(argv[1], O_RDONLY | O_SYNC);
    if (fd == -1) {
        printf("Could NOT open file \"%s\"\n", argv[1]);
        return -1;
    }

    // reading superblock
    read_superblock(fd);
    read_blockgroup(fd);
    read_inodeTable(fd);

    while (1) {
        // extShell waits for one cmd and executes it.
        // returns 0 on successful execution of command,
        // returns -EINVAL on unknown command
        // returns -1 if cmd="q" i.e. quit.
        if (extShell(fd) == -1)
            break;
    }

    printf("\n\nQuitting ext-shell.\n\n");
    return (0);
}
