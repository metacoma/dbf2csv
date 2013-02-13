#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

struct dbf7_header_t {
    char db_type;
    char last_update[3];
    
    uint32_t row_count;

    uint16_t header_siz;
    uint16_t record_siz;

    char zero[2];
    
    char incomplete_transaction;
    char encryption;

    char reserved[12];

    char mdx_flag; 

    char language;
    
    char zero2[2];

    char language_driver[32];

    char reserved2[4];
    
}; 

struct field_descriptor_t {
    char name[32];
    uint8_t type;
    uint8_t length;
    uint8_t decimal_count;
    uint8_t reserved[2];
    uint8_t mdx; 
    uint8_t reserved2[2];
    uint32_t autoincrement;
    uint8_t reserved3[4];
};

int main(int argc, char **argv) {

    int fd;
    char *db_type_desc;
    /*
    char db_type;
    char last_update[3];
    uint32_t row_count;
    uint16_t header_size;
    uint16_t 
    */
    struct dbf7_header_t db_header;
    struct field_descriptor_t field;


    if (argc == 1 || argv[1] == NULL | *argv[1] == 0) {
	fprintf(stderr, "Use %s <file>", argv[0]);
	return EXIT_FAILURE;
    } 

    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
	fprintf(stderr, "can't open src.dbf\n");
	return EXIT_FAILURE;
    } 

    if (read(fd, &db_header, sizeof(db_header)) != sizeof(db_header)) {
	fprintf(stderr, "db_header read err\n");
	close(fd);
	return EXIT_FAILURE;
    } 
    
    //printf("Db type: %d\n", db_header.db_type );

    switch (db_header.db_type) {

		case 0x02: db_type_desc = "FoxBASE"; break;
		case 0x03: db_type_desc = "FoxBASE+/Dbase III plus, no memo"; break;
		case 0x30: db_type_desc = "Visual FoxPro"; break;
		case 0x31: db_type_desc = "Visual FoxPro, autoincrement enabled"; break;
		case 0x32: db_type_desc = "Visual FoxPro with field type Varchar or Varbinary"; break;
		case 0x43: db_type_desc = "dBASE IV SQL table files, no memo"; break;
		case 0x63: db_type_desc = "dBASE IV SQL system files, no memo"; break;
		case 0x83: db_type_desc = "FoxBASE+/dBASE III PLUS, with memo"; break;
		case 0x8B: db_type_desc = "dBASE IV with memo"; break;
		case 0xCB: db_type_desc = "dBASE IV SQL table files, with memo"; break;
		case 0xF5: db_type_desc = "FoxPro 2.x (or earlier) with memo"; break;
		case 0xE5: db_type_desc = "HiPer-Six format with SMT memo file"; break;
		case 0xFB: db_type_desc = "FoxBASE"; break;
		default: db_type_desc = NULL; 
    }; 

    if (db_type_desc == NULL) {
	fprintf(stderr, "Unknown db type: %d\n", db_header.db_type);
	//return EXIT_FAILURE;
    } 

    printf("header siz: %lu\n", sizeof(db_header));

    printf("Last modified: %d/%d/%d\n", db_header.last_update[0] + 1900, db_header.last_update[1], db_header.last_update[2]);
    printf("Row count: %u\n", db_header.row_count);
    printf("Header siz: %u, record siz: %u\n", db_header.header_siz, db_header.record_siz);

    if (read(fd, &field, sizeof(struct field_descriptor_t)) != sizeof(struct field_descriptor_t)) { 
	fprintf(stderr, "field_descriptior_t read err\n");
	close(fd);
	return EXIT_FAILURE;
    } 
    
    
    close(fd);

    return EXIT_SUCCESS;
} 
