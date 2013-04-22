#include <assert.h>
#include <errno.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iconv.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define DBF7_FIELD_DESCRIPTOR_TERMINATOR 0x0D

#define DBF_RECORD_NORMAL ' '
#define DBF_RECORD_DELETED '*'

#define FIELD_D_SIZ 8

#define FAIL_MACROS(cause) \
	if (iconv_buf) \
	    free(iconv_buf); \
	fprintf(stderr, "error %s\n", cause);\
	if (iconv_p)\
	    iconv_close(iconv_p);\
	close(fd);\
	return EXIT_FAILURE;

#define DELIMITER_DEFAULT '|'


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

long get_long(u_char *cp)
{
        long ret;

        ret = *cp++;
        ret += ((*cp++)<<8);
        ret += ((*cp++)<<16);
        ret += ((*cp++)<<24);

        return ret;
}

int32_t dbf_get_int32(char *buf, int32_t *r) {
    char s = (buf[0] ^ 128) ? 1 : -1;
    buf[0] = buf[0] ^ 128;
    memcpy(r, buf, sizeof(int32_t));
    return be32toh(*r) * s;
}

void byte_reverse(char *src, char *dst, int buf_siz) {
    int i, j;
    for (i = 0, j = buf_siz; buf_siz >= 0; i++, buf_siz--) 
	dst[i] = src[j]; 
} 

time_t dbf_get_time(char *buf) {
    double i = 0; 
    static char r[8];
    int j, k;
    char cbuf[16];

    /* reverse, XXX check endiness */
    for (j = 0, k = 7; k >= 0; k--, j++) 
	r[j] = buf[k]; 
    
    memcpy(&i, r, sizeof(r)); 

    if (buf[0] & 0x80) {
	i = -62135686800000.0 - i;
    } else {
	i = -62135686800000.0 + i;
    } 

    snprintf(cbuf, sizeof cbuf - 1, "%.0f\n", i/1000);
    return strtoll(cbuf, 0, 10);
} 

double dbf_get_double(char *buf, size_t buf_siz) {
    double d;
    int j, k;
    char r[8];

    assert(buf_siz == sizeof(double));

    /* reverse, XXX check endiness */
    for (j = 0, k = 7; k >= 0; k--, j++) 
	r[j] = buf[k]; 

    memcpy(&d, r, sizeof(double));

    return d * ((r[0] & 80) ? 1 : - 1);
} 




int main(int argc, char **argv) {

    int fd, i, r;
    /*char *db_type_desc;*/
    struct dbf7_header_t db_header;
    struct field_descriptor_t field;
    struct field_properties_t properties;
    struct standart_properties_t standart;
    uint8_t field_descriptor_terminator;
    char *buf = NULL, *iconv_buf = NULL, *p, c, *_p;
    uint32_t buf_siz = 0, iconv_buf_siz;
    struct field_descriptor_t **field_array;
    uint8_t field_count; 
    uint32_t parsed, j, length;
    //uint32_t *field_date, *field_time;
    long int *field_date, *field_time;
    uint8_t use_langdriver;
    size_t iconv_bytes;
    iconv_t iconv_p = NULL;
    double o;
    int32_t tmp_i;
    void *record;
    
    char **inbuf, **outbuf;
    size_t inbytesleft, outbytesleft;

    struct tm *tm;
    time_t t;

    char out_time_buf[32], rev_buf[8];
    
    char delim = DELIMITER_DEFAULT, opt;
    char *filename;

    char info_only = 0, awk_output = 0;
    char *timestamp_fmt = "%F %H:%M:%S";


    while ((opt = getopt(argc, argv, "d:iat:")) != -1) {
	    switch (opt) {
		case 'd':
		    delim = *optarg;
		break;
		case 'i':
		    info_only = 1;
		break;
		case 'a':
		    awk_output = 1;
		break;
		case 't':
		    timestamp_fmt = optarg;
		break;
	    } 
    } 


    /*
    if (argc == 1 || argv[1] == NULL || *argv[1] == 0) {
	fprintf(stderr, "Use %s <file>", argv[0]);
	return EXIT_FAILURE;
    } 
    */
    
    filename = (argc == 1) ? argv[1] : argv[argc - 1]; 

    fd = open(filename, O_RDONLY);

    if (fd == -1) {
	fprintf(stderr, "can't open %s\n", filename);
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


    if (info_only && !awk_output) {

	printf("header siz: %lu\n", sizeof(db_header));

	printf("Last modified: %d/%d/%d\n", db_header.last_update[0] + 1900, db_header.last_update[1], db_header.last_update[2]);
	printf("Row count: %u\n", db_header.row_count);
	printf("Header siz: %u, record siz: %u\n", db_header.header_siz, db_header.record_siz);

    } 

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

    iconv_buf_siz = 0;
    
    for (i = 0; i < field_count; i++) {
	field_array[i] = malloc(sizeof(struct field_descriptor_t));

	if (read(fd, field_array[i], sizeof(struct field_descriptor_t)) != sizeof(struct field_descriptor_t)) { 
	    fprintf(stderr, "field_descriptior_t read err\n");
	    close(fd);
	    return EXIT_FAILURE;
	} 

	if (info_only) 
	    if (!awk_output) 
		printf("#%d: %s '%c', size: %d\n", i, field_array[i]->name, field_array[i]->type, field_array[i]->length);
	    else
		printf("\t%s = $%d\n", field_array[i]->name, i + 1);

	if (field_array[i]->length > iconv_buf_siz) 
	    iconv_buf_siz = field_array[i]->length;
    } 

    if (info_only) 
	return EXIT_SUCCESS;

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
	//printf("Use langdriver: %s\n", db_header.language);
	iconv_p = iconv_open("UTF-8", "CP1251");

	if (iconv_p == (iconv_t ) -1) {
	    FAIL_MACROS(strerror(errno));
	} 

	assert(iconv_buf_siz);

	iconv_buf_siz = iconv_buf_siz * 4;
	iconv_buf = malloc(iconv_buf_siz);

    } 
    

    record = malloc(db_header.record_siz);

    for (parsed = 0, j = 0; parsed < db_header.record_siz * db_header.row_count; j++) {

	    r = read(fd, record, db_header.record_siz);

	    if (r == -1) {
		FAIL_MACROS(strerror(errno));
	    } 

	    assert( r == db_header.record_siz );

	    p = record;

	    assert( *p == DBF_RECORD_NORMAL || *p == DBF_RECORD_DELETED );

	    parsed += db_header.record_siz;
	    
	    if ( *p == DBF_RECORD_DELETED ) {
		j--;
		continue;
	    } 

	    p++; /* skip record separator */

	    for (i = 0; i < field_count; i++) {
		buf = p;
		buf_siz = field_array[i]->length;


		switch(field_array[i]->type) {
		    case 'I':
			printf("%d", dbf_get_int32(buf, &tmp_i));
		    break;
		    case 'O':
			printf("%.0f", dbf_get_double(buf, buf_siz));
		    break;
		    case 'C':
			_p = buf;
			length = field_array[i]->length;

			if (use_langdriver) {	
			    inbuf = &buf;
			    inbytesleft = length;
			    outbuf = &iconv_buf; 
			    outbytesleft = iconv_buf_siz;
    
			    iconv_bytes = iconv(iconv_p, inbuf, &inbytesleft, outbuf, &outbytesleft);

			    /* restore */
			    *inbuf -= (length - inbytesleft);
			    _p = *outbuf -= (iconv_buf_siz - outbytesleft);
			    length = iconv_buf_siz - outbytesleft;
			    
			    /*
			    buggy?
			    if (iconv_bytes == -1) {
				FAIL_MACROS(strerror(errno));
			    } 
			    */
			} 
			/* remove last spaces */; 
			while ( length && * ((char *) _p + (length - 1)) == ' ') { 
			    length--;
			} 
			printf("%.*s", length, _p);
		    break;
		    case 'N':
			length = field_array[i]->length;
			while ( length && * ((char *) buf + (length - 1)) == ' ') { 
			    length--;
			} 

			/* remove head spaces */
			while (*buf == ' ' && length) {
			    length--;
			    buf++;
			} 

			printf("%.*s", length , buf);
		    break;
		    case 'D':
			assert(field_array[i]->length == FIELD_D_SIZ);
			printf("%c%c%c%c%c%c%c%c ", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
			
			break;
		    case '@':
			if (*p) {
			    t = dbf_get_time(buf);	
			    tm = localtime(&t);	
			    if (tm == NULL) {
				FAIL_MACROS("localtime");
			    } 
			
			    strftime(out_time_buf, sizeof(out_time_buf), timestamp_fmt, tm );
			    printf("%s", out_time_buf);
			} 
			
		    break;
		    case 'L':
			printf("%c", *buf);
		    break;
		    default:
			fprintf(stderr, "Unknown field type: '%c'\n", field_array[i]->type);
		    break;
		} 

		printf("%c", delim);
		p += field_array[i]->length;
	    }
	    printf("\n");

	    //assert( (char *) record + db_header.record_siz == p ); 
    } 

    //free(buf);
    free(iconv_buf);

    for (i = 0; i < field_count; i++) {
	free(field_array[i]);
    } 
    free(field_array); 
    iconv_close(iconv_p);

    close(fd);
    return EXIT_SUCCESS;
} 
