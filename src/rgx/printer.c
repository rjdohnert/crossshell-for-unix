#include "printer.h"
#include "allocation.h"
#include "unicode.h"
#include "search_state.h"
#include <stdarg.h>

static RGX_THREAD_LOCAL uint64_t output_bytes;
static int counted_printf(FILE* stream,const char* format,...) {
    va_list arguments;va_start(arguments,format);int result=vfprintf(stream,format,arguments);va_end(arguments);
    if(result>0 && stream!=stderr)output_bytes+=(uint64_t)result;return result;
}
static int counted_puts(const char* text,FILE* stream) {
    int result=fputs(text,stream);if(result!=EOF && stream!=stderr)output_bytes+=strlen(text);return result;
}
static int counted_putc(int value,FILE* stream) {
    int result=fputc(value,stream);if(result!=EOF && stream!=stderr)++output_bytes;return result;
}
static size_t counted_write(const void* bytes,size_t size,size_t count,FILE* stream) {
    size_t result=fwrite(bytes,size,count,stream);if(stream!=stderr)output_bytes+=result*size;return result;
}
#define fprintf counted_printf
#define fputs counted_puts
#define fputc counted_putc
#define fwrite counted_write

#define ANSI_RED    "\x1b[1;31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_CYAN   "\x1b[36m"
#define ANSI_RESET  "\x1b[0m"

RgxPrinter* RgxPrinter_New(const RgxOptions* opts) {
    RgxPrinter* p = (RgxPrinter*)rgx_calloc(1, sizeof(RgxPrinter));
    p->opts = opts;
    p->hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    p->stream = stdout;
    QueryPerformanceCounter(&p->started);QueryPerformanceFrequency(&p->frequency);

    if (opts->color_mode == COLOR_ALWAYS) {
        p->enable_color = true;
    } else if (opts->color_mode == COLOR_NEVER) {
        p->enable_color = false;
    } else {
        DWORD mode = 0;
        p->enable_color = (GetConsoleMode(p->hOut, &mode) != 0);
    }

    if (p->enable_color) {
        DWORD mode = 0;
        if (GetConsoleMode(p->hOut, &mode)) {
            SetConsoleMode(p->hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    if(opts->json)p->enable_color=false;
    return p;
}

static void json_string(FILE* stream,const char* text,size_t length) {
    fputc('"',stream);
    for(size_t index=0;index<length;++index){unsigned char value=(unsigned char)text[index];
        if(value=='"' || value=='\\'){fputc('\\',stream);fputc(value,stream);}
        else if(value<32)fprintf(stream,"\\u%04x",(unsigned)value);
        else fputc(value,stream);
    }
    fputc('"',stream);
}
static void json_data(FILE* stream,const char* text,size_t length) {
    if(utf8_valid(text,length)){fputs("{\"text\":",stream);json_string(stream,text,length);fputc('}',stream);return;}
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    fputs("{\"bytes\":\"",stream);
    for(size_t index=0;index<length;index+=3){size_t left=length-index;uint32_t value=(uint32_t)(unsigned char)text[index]<<16;
        if(left>1)value|=(uint32_t)(unsigned char)text[index+1]<<8;if(left>2)value|=(unsigned char)text[index+2];
        fputc(alphabet[(value>>18)&63],stream);fputc(alphabet[(value>>12)&63],stream);
        fputc(left>1?alphabet[(value>>6)&63]:'=',stream);fputc(left>2?alphabet[value&63]:'=',stream);
    }
    fputs("\"}",stream);
}
static void json_stats(FILE* stream,uint64_t files,uint64_t bytes,uint64_t lines,uint64_t selected,uint64_t matches,uint64_t matched_files,uint64_t printed,double elapsed) {
    fprintf(stream,"{\"elapsed\":{\"secs\":%llu,\"nanos\":%u,\"human\":\"%.6fs\"},\"searches\":%llu,\"searches_with_match\":%llu,\"bytes_searched\":%llu,\"bytes_printed\":%llu,\"matched_lines\":%llu,\"matches\":%llu,\"lines_searched\":%llu}",
        (unsigned long long)elapsed,(unsigned)((elapsed-(uint64_t)elapsed)*1000000000),elapsed,
        (unsigned long long)files,(unsigned long long)matched_files,(unsigned long long)bytes,(unsigned long long)printed,(unsigned long long)selected,(unsigned long long)matches,(unsigned long long)lines);
}
void printer_begin_file(RgxPrinter* printer,const char* path) {
    (void)path;++printer->files;printer->line_offset=0;printer->file_emitted=printer->heading_emitted=false;
    printer->file_bytes=printer->file_lines=printer->file_matched_lines=printer->file_matches=0;
    printer->file_output_start=output_bytes;QueryPerformanceCounter(&printer->file_started);
}
static void json_announce(RgxPrinter* printer,const char* path) {
    if(printer->file_emitted)return;
    fputs("{\"type\":\"begin\",\"data\":{\"path\":",printer->stream);json_data(printer->stream,path,strlen(path));fputs("}}\n",printer->stream);printer->file_emitted=true;
}
void printer_end_file(RgxPrinter* printer,const char* path) {
    if(printer->file_matched_lines)++printer->files_with_matches;
    uint64_t printed=output_bytes-printer->file_output_start;printer->bytes_printed+=printed;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);double elapsed=(double)(now.QuadPart-printer->file_started.QuadPart)/(double)printer->frequency.QuadPart;
    if(printer->opts->json && printer->file_emitted){fputs("{\"type\":\"end\",\"data\":{\"path\":",printer->stream);json_data(printer->stream,path,strlen(path));
        fputs(",\"binary_offset\":null,\"stats\":",printer->stream);json_stats(printer->stream,1,printer->file_bytes,printer->file_lines,printer->file_matched_lines,printer->file_matches,printer->file_matched_lines?1:0,printed,elapsed);fputs("}}\n",printer->stream);}
}
void printer_merge(RgxPrinter* target,const RgxPrinter* source) {
    target->files+=source->files;target->bytes+=source->bytes;target->lines+=source->lines;
    target->matched_lines+=source->matched_lines;target->matches+=source->matches;
    target->files_with_matches+=source->files_with_matches;
    target->bytes_printed+=source->bytes_printed;
}
void printer_finish(RgxPrinter* printer) {
    if(printer->opts->quiet)return;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);double elapsed=(double)(now.QuadPart-printer->started.QuadPart)/(double)printer->frequency.QuadPart;
    if(printer->opts->json){fprintf(printer->stream,"{\"type\":\"summary\",\"data\":{\"elapsed_total\":{\"secs\":%llu,\"nanos\":%u,\"human\":\"%.6fs\"},\"stats\":",
        (unsigned long long)elapsed,(unsigned)((elapsed-(uint64_t)elapsed)*1000000000),elapsed);json_stats(printer->stream,printer->files,printer->bytes,printer->lines,printer->matched_lines,printer->matches,printer->files_with_matches,printer->bytes_printed,elapsed);fputs("}}\n",printer->stream);}
    else if(printer->opts->stats)fprintf(printer->stream,"\n%llu matches\n%llu matched lines\n%llu files searched\n%llu bytes searched\n%.6f seconds\n",(unsigned long long)printer->matches,(unsigned long long)printer->matched_lines,(unsigned long long)printer->files,(unsigned long long)printer->bytes,elapsed);
}
static void json_record(RgxPrinter* self,const char* path,size_t number,const char* line,size_t length,MatchSpan span,bool match) {
    json_announce(self,path);
    fprintf(self->stream,"{\"type\":\"%s\",\"data\":{\"path\":",match?"match":"context");json_data(self->stream,path,strlen(path));
    fputs(",\"lines\":",self->stream);
    if(self->line_terminated && (!length || line[length-1]!='\n')){
        char* terminated=(char*)rgx_malloc(length+1);if(length)memcpy(terminated,line,length);terminated[length]='\n';json_data(self->stream,terminated,length+1);free(terminated);
    }else json_data(self->stream,line,length);
    fprintf(self->stream,",\"line_number\":%zu,\"absolute_offset\":%zu,\"submatches\":[",number,self->line_offset);
    if(match && !self->opts->invert_match){size_t offset=0;bool first=true;
        do {if(!first)fputc(',',self->stream);first=false;fputs("{\"match\":",self->stream);json_data(self->stream,line+span.start,span.length);fprintf(self->stream,",\"start\":%zu,\"end\":%zu}",span.start,span.start+span.length);
            offset=span.start+span.length;if(!span.length){if(offset==length)break;size_t width;utf8_rune(line+offset,length-offset,&width);offset+=width;}
        }while(self->matcher && offset<=length && self->matcher->vptr->FindMatch(self->matcher,line,length,offset,&span));
    }
    fputs("]}}\n",self->stream);
}
static void print_prefix(RgxPrinter* self,const char* path,size_t number,MatchSpan span,bool print_path,bool match) {
    char separator=match?':':'-';
    if(print_path && self->opts->heading && !self->heading_emitted){fprintf(self->stream,"%s\n",path);self->heading_emitted=true;}
    if(print_path && !self->opts->heading){if(self->enable_color)fprintf(self->stream,"%s%s%s",ANSI_CYAN,path,ANSI_RESET);else fputs(path,self->stream);fputc(self->opts->null_paths?0:separator,self->stream);}
    if(self->opts->show_line_numbers)fprintf(self->stream,"%zu%c",number,separator);
    if(self->opts->column && match)fprintf(self->stream,"%zu%c",span.start+1,separator);
    if(self->opts->byte_offset)fprintf(self->stream,"%zu%c",self->line_offset+(self->opts->only_matching?span.start:0),separator);
}
static void write_replacement(RgxPrinter* self,const char* match,size_t length) {
    const char* cursor=self->opts->replacement;
    while(*cursor){if(cursor[0]=='$' && cursor[1]=='0'){fwrite(match,1,length,self->stream);cursor+=2;}else if(cursor[0]=='$' && cursor[1]=='$'){fputc('$',self->stream);cursor+=2;}else fputc(*cursor++,self->stream);}
}

void RgxPrinter_PrintPath(RgxPrinter* self, const char* path) {
    if(self->enable_color)fprintf(self->stream,"%s%s%s",ANSI_CYAN,path,ANSI_RESET);else fputs(path,self->stream);
    fputc(self->opts->null_paths?0:'\n',self->stream);
}

void RgxPrinter_PrintCount(RgxPrinter* self, const char* path, size_t count, bool print_path) {
    if (print_path) {
        fputs(path,self->stream);fputc(self->opts->null_paths?0:':',self->stream);fprintf(self->stream,"%zu\n",count);
    } else {
        fprintf(self->stream,"%zu\n",count);
    }
}

void RgxPrinter_PrintMatch(RgxPrinter* self, const char* path, size_t line_num, const char* line, size_t len, MatchSpan span, bool print_path) {
    if(self->stream!=stdout && _ftelli64(self->stream)>=64LL*1024*1024){rgx_had_error=rgx_match_error=true;fprintf(stderr,"rgx: worker output exceeds 64 MiB\n");return;}
    if (span.start > len || span.length > len - span.start) {
        fprintf(stderr, "rgx: invalid match span\n");
        exit(2);
    }
    if(self->opts->json){json_record(self,path,line_num,line,len,span,true);return;}
    if (self->opts->only_matching && !span.length) return;
    print_prefix(self,path,line_num,span,print_path,true);

    if (self->opts->only_matching) {
        if (span.length > 0 && span.start < len) {
            if(self->opts->replacement)write_replacement(self,line+span.start,span.length);
            else {if(self->enable_color)fputs(ANSI_RED,self->stream);fwrite(line+span.start,1,span.length,self->stream);if(self->enable_color)fputs(ANSI_RESET,self->stream);}
            fputc('\n',self->stream);
        }
        return;
    }

    if(self->opts->replacement) {
        size_t consumed=0,offset=0;
        do {
            fwrite(line+consumed,1,span.start-consumed,self->stream);write_replacement(self,line+span.start,span.length);
            consumed=span.start+span.length;offset=consumed;
            if(!span.length){if(offset==len)break;size_t width;utf8_rune(line+offset,len-offset,&width);offset+=width;}
        }while(self->matcher && offset<=len && self->matcher->vptr->FindMatch(self->matcher,line,len,offset,&span));
        fwrite(line+consumed,1,len-consumed,self->stream);fputc('\n',self->stream);
    } else if (self->enable_color && span.start <= len) {
        fwrite(line,1,span.start,self->stream);
        fputs(ANSI_RED,self->stream);fwrite(line+span.start,1,span.length,self->stream);fputs(ANSI_RESET,self->stream);
        fwrite(line+span.start+span.length,1,len-span.start-span.length,self->stream);fputc('\n',self->stream);
    } else {
        fwrite(line,1,len,self->stream);fputc('\n',self->stream);
    }
}

void RgxPrinter_PrintContext(RgxPrinter* self, const char* path, size_t line_num, const char* line, size_t len, bool print_path) {
    if(self->stream!=stdout && _ftelli64(self->stream)>=64LL*1024*1024){rgx_had_error=rgx_match_error=true;fprintf(stderr,"rgx: worker output exceeds 64 MiB\n");return;}
    if(self->opts->json){json_record(self,path,line_num,line,len,(MatchSpan){0},false);return;}
    print_prefix(self,path,line_num,(MatchSpan){0},print_path,false);fwrite(line,1,len,self->stream);fputc('\n',self->stream);
}

void RgxPrinter_Free(RgxPrinter* self) {
    if (self) free(self);
}

/* ========================================================================= */
/*                      4. FILE SEARCH ENGINE                                */
/* ========================================================================= */
