#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define DBF7_FIELD_DESCRIPTOR_TERMINATOR 0x0D

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

struct field_properties_t {
    uint16_t standart;		/* Number of Standard Properties */
    uint16_t standart_start;	/* Start of Standard Property Descriptor Array. */ 
    uint16_t custom;		/* Number of Custom Properties. */
    uint16_t custom_start;	/* Start of Custom Property Descriptor Array. */
    uint16_t ri;		/* Number of Referential Integrity (RI) properties. */
    uint16_t ri_start;		/* Start of RI Property Descriptor Array */
    uint16_t data_start;	/* Start of data - this points past the Descriptor arrays to data used by the arrays - for example Custom property names are stored here */
    uint16_t actual_size;	/* Actual size of structure, including data (Note: in the .DBF this will be padded with zeroes to the nearest 0x200, and may have 0x1A at the end). If the structure contains RI data, it will not be padded. */
}; 

int main(int argc, char **argv) {

    int fd, i;
    char *db_type_desc;
    struct dbf7_header_t db_header;
    struct field_descriptor_t field;
    uint8_t field_descriptor_terminator;


    if (argc == 1 || argv[1] == NULL || *argv[1] == 0) {
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

    if ( (db_header.header_siz - (sizeof(struct dbf7_header_t) + 1) ) % sizeof(struct field_descriptor_t) != 0 ) {
	fprintf(stderr, "db header size mistmatch\n");
	close(fd);
	return EXIT_FAILURE;
    } 

    for (i = 0; i < (db_header.header_siz - (sizeof(struct dbf7_header_t) + 1)) / sizeof(struct field_descriptor_t) ; i++) {
	if (read(fd, &field, sizeof(struct field_descriptor_t)) != sizeof(struct field_descriptor_t)) { 
	    fprintf(stderr, "field_descriptior_t read err\n");
	    close(fd);
	    return EXIT_FAILURE;
	} 
	printf("#%d: %s '%c'\n", i, field.name, field.type);
    } 
    
    if (read(fd, &field_descriptor_terminator, sizeof(field_descriptor_terminator)) != sizeof(field_descriptor_terminator)) {
        fprintf(stderr, "field_descriptor_terminator read err\n");
        close(fd);
        return EXIT_FAILURE;
    } 


    if (field_descriptor_terminator != DBF7_FIELD_DESCRIPTOR_TERMINATOR) {
	fprintf(stderr, "field_descriptor_terminator error\n");
	close(fd);
    } 

    printf("terminator: 0x%x\n", field_descriptor_terminator);
    

    return EXIT_SUCCESS;
} 
