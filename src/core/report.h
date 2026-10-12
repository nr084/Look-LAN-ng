#ifndef REPORT_H
#define REPORT_H
#include "profile.h"
#include "host.h"
#include "vendor.h"
typedef enum { REPORT_CSV, REPORT_TXT, REPORT_HTML } report_format_t;
typedef struct { bool russian; char profile_name[256], copyright[64]; } report_options_t;
bool report_write(report_format_t format, const lan_host_t *hosts, size_t count, const vendor_table_t *vendors, const report_options_t *options, text_writer_t writer, void *context);
#endif
