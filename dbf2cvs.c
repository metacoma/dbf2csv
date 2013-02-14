#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iconv.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define DBF7_FIELD_DESCRIPTOR_TERMINATOR 0x0D

#define FAIL_MACROS(cause) \
	if (buf) \
	    free(buf); \
	fprintf(stderr, "error %s\n", cause);\
	close(fd); \
	return EXIT_FAILURE;

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

struct standart_properties_t { 
    uint16_t generational_number; /* Generational number. More than one value may exist for a property. The current value is the value with the highest generational number. */
    uint16_t table_field_offset; /* Table field offset - base one. 01 for the first field in the table, 02 for the second field, etc. Note: this will be 0 in the case of a constraint.  */
    uint8_t property_type; /* Which property is described in this record  */
    uint8_t field_type; 
    uint8_t array_element; /* 0x00 if the array element is a constraint, 0x02 otherwise. */
    uint8_t reserved[4];
    uint16_t offset; /* Offset from the start of this structure to the data for the property. The Required property has no data associated with it, so it is always 0. */
    uint16_t width; /* Width of database field associated with the property, and hence size of the data (includes 0 terminator in the case of a constraint). */

};


int main(int argc, char **argv) {

    int fd, i, r;
    /*char *db_type_desc;*/
    struct dbf7_header_t db_header;
    struct field_descriptor_t field;
    struct field_properties_t properties;
    struct standart_properties_t standart;
    uint8_t field_descriptor_terminator;
    char *buf = NULL, *iconv_buf = NULL, *p;
    uint32_t buf_siz = 0, iconv_buf_siz;
    struct field_descriptor_t **field_array;
    uint8_t field_count; 
    uint32_t parsed, j, length;
    uint32_t *field_date, *field_time;
    uint8_t use_langdriver;
    size_t iconv_bytes;
    iconv_t iconv_p;
    
    char **inbuf, **outbuf;
    size_t inbytesleft, outbytesleft;

    if (argc == 1 || argv[1] == NULL || *argv[1] == 0) {
	fprintf(stderr, "Use %s <file>", argv[0]);
	return EXIT_FAILURE;
    } 

    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
	fprintf(stderr, "can't open src.dbf\n");
	return EXIT_FAILURE;
    } 

    memset(&db_header, 0, sizeof(struct dbf7_header_t));
    memset(&field, 0, sizeof(struct field_descriptor_t));
    memset(&properties, 0, sizeof(struct field_properties_t));

    if (read(fd, &db_header, sizeof(db_header)) != sizeof(db_header)) {
	fprintf(stderr, "db_header read err\n");
	close(fd);
	return EXIT_FAILURE;
    } 
    
    //printf("Db type: %d\n", db_header.db_type );

    /*
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
    */

    printf("header siz: %lu\n", sizeof(db_header));

    printf("Last modified: %d/%d/%d\n", db_header.last_update[0] + 1900, db_header.last_update[1], db_header.last_update[2]);
    printf("Row count: %u\n", db_header.row_count);
    printf("Header siz: %u, record siz: %u\n", db_header.header_siz, db_header.record_siz);

    if ( (db_header.header_siz - (sizeof(struct dbf7_header_t) + 1) ) % sizeof(struct field_descriptor_t) != 0 ) {
	fprintf(stderr, "db header size mistmatch\n");
	close(fd);
	return EXIT_FAILURE;
    } 

    field_count = (db_header.header_siz - (sizeof(struct dbf7_header_t) + 1)) / sizeof(struct field_descriptor_t);

    field_array = malloc( sizeof (struct field_descriptor_t *) * field_count);

    if (field_array == NULL) {
	fprintf(stderr, "malloc failed\n");
	close(fd);
	return EXIT_FAILURE;
    } 

    buf = malloc(256);
    buf_siz = 256;
    
    for (i = 0; i < field_count; i++) {
	field_array[i] = malloc(sizeof(struct field_descriptor_t));
	/*if (read(fd, &field, sizeof(struct field_descriptor_t)) != sizeof(struct field_descriptor_t)) { */
	if (read(fd, field_array[i], sizeof(struct field_descriptor_t)) != sizeof(struct field_descriptor_t)) { 
	    fprintf(stderr, "field_descriptior_t read err\n");
	    close(fd);
	    return EXIT_FAILURE;
	} 
	printf("#%d: %s '%c', size: %d\n", i, field_array[i]->name, field_array[i]->type, field_array[i]->length);
	if (field_array[i]->length > buf_siz) {
	    buf_siz = field_array[i]->length;
	    (void) realloc(buf, buf_siz);
	    if (buf == NULL) {
		fprintf(stderr, "realloc failed\n");
		close(fd);
		return EXIT_FAILURE;
	    } 
	} 
    } 

    if (read(fd, &field_descriptor_terminator, sizeof(field_descriptor_terminator)) != sizeof(field_descriptor_terminator)) {
        fprintf(stderr, "field_descriptor_terminator read err\n");
        close(fd);
        return EXIT_FAILURE;
    } 



    if (field_descriptor_terminator != DBF7_FIELD_DESCRIPTOR_TERMINATOR) {
	fprintf(stderr, "field_descriptor_terminator error\n");
	close(fd);
        return EXIT_FAILURE;
    } 

    use_langdriver = (*db_header.language_driver) ? 1 : 0;

    if (use_langdriver) {
	iconv_p = iconv_open("UTF-8", "CP1251");

	if (iconv_p == (iconv_t ) -1) {
	    FAIL_MACROS(strerror(errno));
	} 

	iconv_buf_siz = buf_siz * 4;
	iconv_buf = malloc(iconv_buf_siz);

    } 
    

    for (parsed = 0, j = 0; parsed < db_header.record_siz * db_header.row_count; j++) {
	    printf("#%u.", j);
	    r = read(fd, buf, 1);
	    if (r != 1) {
		fprintf(stderr, "err read record separator\n");
		close(fd);
		return EXIT_FAILURE;
	    } 
	    parsed += r;
	    for (i = 0; i < field_count; i++) { 
		memset(buf, 0, buf_siz);
		r = read(fd, buf, field_array[i]->length);
		if (r != field_array[i]->length) {
		    fprintf(stderr, "record read err: %d\n", r);
		    close(fd);
		    return EXIT_FAILURE;
		} 
		parsed += r;
		switch(field_array[i]->type) {
		    case 'I':
			printf("'%ld'(%d) ", (int32_t *) buf, field_array[i]->length);
		    break;
		    case 'C':
			p = buf;
			length = field_array[i]->length;
			if (use_langdriver) {	
			    inbuf = &buf;
			    inbytesleft = length;
			    outbuf = &iconv_buf; 
			    outbytesleft = iconv_buf_siz;
    
			    iconv_bytes = iconv(iconv_p, inbuf, &inbytesleft, outbuf, &outbytesleft);

			    /* restore */
			    *inbuf -= (length - inbytesleft);
			    p = *outbuf -= (iconv_buf_siz - outbytesleft);
			    length = iconv_buf_siz - outbytesleft;
			    
			    /*
			    buggy?
			    if (iconv_bytes == -1) {
				FAIL_MACROS(strerror(errno));
			    } 
			    */
			} 
			printf("'%s'(%d) ", p, length);
		    break;
		    case '@':
			field_date = (uint32_t *) buf; 
			field_time = (uint32_t *) ((char *) buf + sizeof(uint32_t)); 
			printf("%lu/%lu(%d)", field_date, field_time, field_array[i]->length);
		    break;
		    case 'L':
			printf("'%c'(%d) ", *buf, field_array[i]->length);
		    break;
		    default:
			fprintf(stderr, "Unknown field type: '%c'\n", field_array[i]->type);
		    break;
		} 
	    } 
	    printf("\n");
    } 

    close(fd);
    return EXIT_SUCCESS;
} 
