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

    struct ext2_inode *inode = &inodes[inode_index];

    printf("Inode %d:\n", inode_index);
    printf("  Modo: %u\n", inode->i_mode);
    printf("  UID: %u\n", inode->i_uid);
    printf("  Tamanho: %u bytes\n", inode->i_size);
    printf("  Último acesso: %u\n", inode->i_atime);
    printf("  Criação: %u\n", inode->i_ctime);
    printf("  Modificação: %u\n", inode->i_mtime);
    printf("  Exclusão: %u\n", inode->i_dtime);
    printf("  Contagem de links: %u\n", inode->i_links_count);
    printf("  Número de blocos: %u\n", inode->i_blocks);
    
    printf("  Blocos apontados: ");
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        printf("%u ", inode->i_block[i]);
    }
    printf("\n");
}




int findInodeByName(int fd, int base_inode_num, char* filename, int *filetype) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    // Localiza a tabela de inodes no grupo de blocos
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);
    off_t inode_offset = inode_table_offset + (base_inode_num - 1) * sizeof(struct ext2_inode);

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
    printf("Inode_num stat: %d", inode_num);


    if (inode_num == -1) {
        printf("[ERRO] Arquivo ou diretório não encontrado: %s\n", filename);
        return -1;
    }

    if (inode_num < 0 || inode_num >= superblock->s_inodes_per_group) {
        printf("[ERRO] Número de inode inválido (%d).\n", inode_num);
        return -1;
    }

    struct ext2_inode *inode = &inodes[inode_num];

    // Renomeando a variável para evitar conflito com a função ctime()
    time_t access_time = inode->i_atime;
    time_t modify_time = inode->i_mtime;
    time_t change_time = inode->i_ctime;

    // Convertendo timestamps
    char *atime_str = strtok(ctime(&access_time), "\n");
    char *mtime_str = strtok(ctime(&modify_time), "\n");
    char *ctime_str = strtok(ctime(&change_time), "\n");

    printf("\n======= Metadados do Arquivo: %s =======\n", filename);
    printFileType(filetype);
    printf("Modo: %o\n", inode->i_mode);
    printf("Número de links: %u\n", inode->i_links_count);
    printf("UID do dono: %u\n", inode->i_uid);
    printf("GID do grupo: %u\n", inode->i_gid);
    printf("Tamanho: %u bytes\n", inode->i_size);
    printf("Último acesso: %s\n", atime_str);
    printf("Última modificação: %s\n", mtime_str);
    printf("Última alteração nos metadados: %s\n", ctime_str);
    printf("Blocos alocados: %u\n", inode->i_blocks);

    printf("Blocos apontados: ");
    for (int j = 0; j < EXT2_N_BLOCKS; j++) {
        printf("%u ", inode->i_block[j]);
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
    blockgroup = malloc(sizeof(struct ext2_group_desc));
    if (!blockgroup) {
        perror("Erro ao alocar memória para grupo de blocos");
        return;
    }

    // O descritor do grupo de blocos começa logo após o superbloco
    off_t blockgroup_offset = BASE_OFFSET + BLOCK_SIZE;
    if (lseek(fd, blockgroup_offset, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro no grupo de blocos");
        free(blockgroup);
        return;
    }

    ssize_t bytes_read = read(fd, blockgroup, sizeof(struct ext2_group_desc));
    if (bytes_read != sizeof(struct ext2_group_desc)) {
        perror("Erro ao ler grupo de blocos");
        free(blockgroup);
        return;
    }
}



void read_inodeTable(int fd) {
    size_t inode_table_size = superblock->s_inodes_per_group * superblock->s_inode_size;
    inodes = malloc(inode_table_size);
    if (!inodes) {
        perror("Erro ao alocar memória para tabela de inodes");
        return;
    }

    // O deslocamento correto é o início da tabela de inodes do grupo de blocos
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);
    if (lseek(fd, inode_table_offset, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro na tabela de inodes");
        free(inodes);
        return;
    }

    ssize_t bytes_read = read(fd, inodes, inode_table_size);
    if (bytes_read != inode_table_size) {
        perror("Erro ao ler a tabela de inodes");
        free(inodes);
        return;
    }

}


void ls(int fd, int base_inode_num) {
    struct ext2_inode inode;
    struct ext2_dir_entry entry;
    off_t offset;
    ssize_t bytes_read;

    // Localiza a tabela de inodes no grupo de blocos
    off_t inode_table_offset = BLOCK_OFFSET(blockgroup->bg_inode_table);

    // Calcula o deslocamento do inode dentro da tabela
    off_t inode_offset = inode_table_offset + (base_inode_num - 1) * sizeof(struct ext2_inode);

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

    int filetype;
    int inode_num = findInodeByName(fd, base_inode_num, dirname, &filetype);
    printf("Inode_num cd: %d", inode_num);

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



int extShell(int fd) {
    char cmd[10];  // Suporta comandos de até 9 letras + '\0'
    char filename[EXT2_NAME_LEN + 1];

    while (1) {
        printf("ext-shell$ ");
        scanf("%9s", cmd);  // Limita entrada a 9 caracteres para evitar buffer overflow

        if (!strcmp(cmd, "q")) {
            return -1;
        } else if (!strcmp(cmd, "sb")) {
            sb();  // Exibe informações do super bloco
        } else if (!strcmp(cmd, "ls")) {
            ls(fd, current_directory_inode);  // Lista arquivos a partir do diretório raiz
        } else if (!strcmp(cmd, "stat")) {
            // Solicita o nome do arquivo/diretório a ser examinado
            printf("Digite o nome do arquivo ou diretório: ");
            scanf("%255s", filename);  // Lê o nome do arquivo (máx 255 caracteres)
            statFile(fd, EXT2_ROOT_INODE, filename);  // Chama a função statFile
        } else if (!strcmp(cmd, "cd")) {
            printf("Digite o nome do diretório: ");
            scanf("%255s", filename);
            cd(fd, current_directory_inode, filename);
        }else {
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
