/*
https://github.com/TheCodeArtist/ext2-parser

#gerar ponteiros das estruturas .h

#fazer uma funcao para ler todo o super blcoo de uma vez com lseek e read

#fazer uma funcao para ler todo o desc de uma vez com lseek e read

#com a informacao do superblcoo de quantod indoes eu tenho, faca um for para ler a tabela de indoes

#imprimir as informacoes do super bloco, dar um print em cada variavel da estrutura (sb)

#ls
 
#stat (precisar fazer uma funcao q acha um inode a aprtir do nome dele)

#cd 

#find ultima e pedir para o gpt fazer

#define EXT2_ROOT_INODE 2*/

#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "structures.h"


struct ext2_super_block *superblock;
struct ext2_group_desc *blockgroup;
struct ext2_inode *inodes;
struct ext2_dir_entry *dir;

int findInodeByName(int fd, int base_inode_num, char* filename, int filetype) {
    char* name;
    int curr_inode_num;
    int curr_inode_type;

    struct os_direntry_t* dirEntry = malloc(sizeof(struct os_direntry_t));
    if (!dirEntry) {
        perror("Erro ao alocar memória para entrada de diretório");
        return -1;
    }

    if (lseek(fd, (off_t)(inodes[base_inode_num-1].i_block[0]*1024), SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro");
        free(dirEntry);
        return -1;
    }

    ssize_t bytes_read = read(fd, dirEntry, sizeof(struct os_direntry_t));
    if (bytes_read != sizeof(struct os_direntry_t)) {
        perror("Erro ao ler entrada de diretório");
        free(dirEntry);
        return -1;
    }

    while (dirEntry->inode) {
        name = malloc(dirEntry->name_len + 1);
        if (!name) {
            perror("Erro ao alocar memória para nome do arquivo");
            free(dirEntry);
            return -1;
        }
        memcpy(name, dirEntry->file_name, dirEntry->name_len);
        name[dirEntry->name_len] = '\0';

        curr_inode_num = dirEntry->inode;
        curr_inode_type = dirEntry->file_type;

        if (filetype == curr_inode_type && strcmp(name, filename) == 0) {
            free(name);
            free(dirEntry);
            return curr_inode_num;
        }

        free(name);
        if (lseek(fd, dirEntry->rec_len - sizeof(struct os_direntry_t), SEEK_CUR) == -1) {
            perror("Erro ao avançar na entrada de diretório");
            free(dirEntry);
            return -1;
        }

        bytes_read = read(fd, dirEntry, sizeof(struct os_direntry_t));
        if (bytes_read != sizeof(struct os_direntry_t)) {
            perror("Erro ao ler próxima entrada de diretório");
            free(dirEntry);
            return -1;
        }
    }

    free(dirEntry);
    return -1;
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
    blockgroup = malloc(sizeof(struct os_blockgroup_descriptor_t));
    if (!blockgroup) {
        perror("Erro ao alocar memória para grupo de blocos");
        return;
    }

    if (lseek(fd, (off_t)2048, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro no grupo de blocos");
        free(blockgroup);
        return;
    }

    ssize_t bytes_read = read(fd, blockgroup, sizeof(struct os_blockgroup_descriptor_t));
    if (bytes_read != sizeof(struct os_blockgroup_descriptor_t)) {
        perror("Erro ao ler grupo de blocos");
        free(blockgroup);
        return;
    }
}


void read_inodeTable(int fd) {
    inodes = malloc(superblock->s_inodes_count * superblock->s_inode_size);
    if (!inodes) {
        perror("Erro ao alocar memória para tabela de inodes");
        return;
    }

    if (lseek(fd, (off_t)(blockgroup->bg_inode_table * 1024), SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro na tabela de inodes");
        free(inodes);
        return;
    }

    ssize_t bytes_read = read(fd, inodes, superblock->s_inodes_count * superblock->s_inode_size);
    if (bytes_read != superblock->s_inodes_count * superblock->s_inode_size) {
        perror("Erro ao ler a tabela de inodes");
        free(inodes);
        return;
    }
}

void printInodeType(int inode_type)
{
	switch(inode_type)
	{
	case 1:
		printf("-");
		break;
	case 2:
		printf("d");
		break;
	case 3:
		printf("c");
		break;
	case 4:
		printf("b");
		break;
	case 5:
		printf("B");
		break;
	case 6:
		printf("S");
		break;
	case 7:
		printf("l");
		break;
	default:
		printf("X");
		break;
	}
}


int cd(int fd, int base_inode_num) {
    char dirname[255];
    int ret;

    scanf("%254s", dirname);

    ret = findInodeByName(fd, base_inode_num, dirname, EXT2_FT_DIR);
    if(ret == -1) {
        printf("Directory %s does not exist\n", dirname);
        return base_inode_num;
    } else {
        printf("Now in directory %s\n", dirname);
        return ret;
    }
}
void ls(int fd, int base_inode_num) {
    struct os_inode_t* dir_inode = &inodes[base_inode_num - 1]; // Obtém o inode do diretório

    if (lseek(fd, dir_inode->i_block[0] * 1024, SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro no bloco do diretório");
        return;
    }

    char buffer[1024]; // Buffer para armazenar o bloco do diretório
    ssize_t bytes_read = read(fd, buffer, 1024);
    if (bytes_read <= 0) {
        perror("Erro ao ler o bloco do diretório");
        return;
    }

    struct os_direntry_t* dirEntry = (struct os_direntry_t*)buffer;
    ssize_t offset = 0;

    printf("Inode\tNome\n");
    while (offset < bytes_read) {
        if (dirEntry->inode == 0) break; // Entrada vazia

        char name[dirEntry->name_len + 1];
        memcpy(name, dirEntry->file_name, dirEntry->name_len);
        name[dirEntry->name_len] = '\0';

        // Ignora "." e ".."
        if (!(name[0] == '.' && (name[1] == '.' || name[1] == '\0'))) {
            printf("%d\t%s\n", dirEntry->inode, name);
        }

        offset += dirEntry->rec_len;
        dirEntry = (struct os_direntry_t*)(buffer + offset);
    }
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
    printf("\nNúmero total de blocos: %u", superblock->s_blocks_count);
    printf("\nNúmero de blocos livres: %u", superblock->s_free_blocks_count);
    printf("\nNúmero de inodes livres: %u", superblock->s_free_inodes_count);
    printf("\nPrimeiro inode disponível: %u", superblock->s_first_ino);
    printf("\nVersão do sistema de arquivos: %u", superblock->s_rev_level);
    printf("\nUUID do sistema de arquivos: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", superblock->s_uuid[i]);
    }
    printf("\nNome do volume: %.16s", superblock->s_volume_name);
    printf("\nÚltimo tempo de montagem: %u", superblock->s_mtime);
    printf("\nÚltimo tempo de escrita: %u", superblock->s_wtime);
    printf("\n================================\n");
}

void stat(int fd, int base_inode_num, char* filename, int filetype) {
    int inode_num = findInodeByName(fd, base_inode_num, filename, filetype);
    if (inode_num == -1) {
        printf("Arquivo ou diretório '%s' não encontrado.\n", filename);
        return;
    }

    ext2_inode* inode = &inodes[inode_num - 1];

    printf("Metadados de '%s':\n", filename);
    printf("Número do Inode: %d\n", inode_num);
    printf("Tamanho: %d bytes\n", inode->i_size);
    printf("Blocos: %d\n", inode->i_blocks);
    printf("Permissões: %o\n", inode->i_mode & 0xFFF);
    printf("UID: %d\n", inode->i_uid);
    printf("GID: %d\n", inode->i_gid);
    printf("Número de links: %d\n", inode->i_links_count);
    printf("Tempo de criação: %d\n", inode->i_ctime);
    printf("Tempo de modificação: %d\n", inode->i_mtime);
    printf("Tempo de acesso: %d\n", inode->i_atime);
}

void find(int fd, int base_inode_num, int depth) {
    struct os_direntry_t* dirEntry = malloc(sizeof(struct os_direntry_t));
    if (!dirEntry) {
        perror("Erro ao alocar memória para entrada de diretório");
        return;
    }

    if (lseek(fd, (off_t)(inodes[base_inode_num-1].i_block[0]*1024), SEEK_SET) == -1) {
        perror("Erro ao posicionar ponteiro na lista de diretórios");
        free(dirEntry);
        return;
    }

    ssize_t bytes_read = read(fd, dirEntry, sizeof(struct os_direntry_t));
    if (bytes_read != sizeof(struct os_direntry_t)) {
        perror("Erro ao ler entrada de diretório");
        free(dirEntry);
        return;
    }

    while (dirEntry->inode) {
        char* name = malloc(dirEntry->name_len + 1);
        if (!name) {
            perror("Erro ao alocar memória para nome do arquivo");
            free(dirEntry);
            return;
        }
        memcpy(name, dirEntry->file_name, dirEntry->name_len);
        name[dirEntry->name_len] = '\0';

        for (int i = 0; i < depth; i++) printf("  "); // Indentação
        printInodeType(dirEntry->file_type);
        printf(" %s\n", name);

        if (dirEntry->file_type == EXT2_FT_DIR && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            find(fd, dirEntry->inode, depth + 1);
        }

        free(name);
        if (lseek(fd, dirEntry->rec_len - sizeof(struct os_direntry_t), SEEK_CUR) == -1) {
            perror("Erro ao avançar na entrada de diretório");
            free(dirEntry);
            return;
        }
        bytes_read = read(fd, dirEntry, sizeof(struct os_direntry_t));
        if (bytes_read != sizeof(struct os_direntry_t)) {
            perror("Erro ao ler próxima entrada de diretório");
            free(dirEntry);
            return;
        }
    }

    free(dirEntry);
}



int extShell(int fd) {
    char cmd[5]; // Agora suporta comandos de até 4 letras + '\0'
    static int pwd_inode = 2;

    while (1) {
        printf("ext-shell$ ");
        scanf("%4s", cmd); // Limita entrada a 4 caracteres para evitar buffer overflow

        debug("cmd=%s\n", cmd);

        if (!strcmp(cmd, "q")) {
            return -1;
        } else if (!strcmp(cmd, "ls")) {
            ls(fd, pwd_inode);
        } else if (!strcmp(cmd, "cd")) {
            pwd_inode = cd(fd, pwd_inode);
        } else if (!strcmp(cmd, "cp")) {
            cp(fd, pwd_inode);
        } else if (!strcmp(cmd, "find")) {
            find(fd, pwd_inode, 0); // Começa a busca do diretório atual
        }else if (!strcmp(cmd, "st")) { // Novo comando "stat"
            printf("Informe o nome do arquivo/diretório: ");
            scanf("%255s", filename); // Lê o nome do arquivo, evitando overflow
            stat_file(fd, EXT2_ROOT_INODE, filename, EXT2_FT_REG_FILE); // Busca arquivo
        } 
        else {
            printf("Unknown command: %s\n", cmd);
            continue; // Continua o loop em vez de retornar erro
        }
    }
}


int main(int argc, char **argv)
{
	// open up the disk file
	if (argc !=2) {
	printf("usage:  ext-shell <file.img>\n");
		return -1; 
	}

	int fd = open(argv[1], O_RDONLY|O_SYNC);
	if (fd == -1) {
		printf("Could NOT open file \"%s\"\n", argv[1]);
		return -1; 
	}

	// reading superblock
	read_superblock(fd);
	printf("block size \t\t= %d bytes\n", 1<<(10 + superblock->s_log_block_size));
	printf("inode count \t\t= 0x%x\n", superblock->s_inodes_count);
	printf("inode size \t\t= 0x%x\n", superblock->s_inode_size);

	// reading blockgroup
	read_blockgroup(fd);
	printf("inode table address \t= 0x%x\n", blockgroup->bg_inode_table);
	printf("inode table size \t= %dKB\n", (superblock->s_inodes_count*superblock->s_inode_size)>>10);

	// reading inode table
	read_inodeTable(fd);

	while(1) {
		// extShell waits for one cmd and executes it.
		// returns 0 on successfull execution of command,
		// returns -EINVAL on unknown command
		// returns -1 if cmd="q" i.e. quit.
		if ( extShell(fd)==-1 )
			break;
	}

	printf("\n\nQuitting ext-shell.\n\n");
	return(0);
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