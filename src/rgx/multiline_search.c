#include "multiline_search.h"
#include "text_reader.h"
#include "allocation.h"
#include "unicode.h"
#include "search_state.h"

static size_t line_at(const size_t* starts,size_t count,size_t position) {
    size_t low=0,high=count;
    while(low+1<high){size_t middle=low+(high-low)/2;if(starts[middle]<=position)low=middle;else high=middle;}
    return low;
}

void search_multiline(FILE* stream,const char* path,const RgxOptions* options,IMatcher* matcher,RgxPrinter* printer,bool multiple_targets) {
    printer_begin_file(printer,path);rgx_match_error=false;
    printer->matcher=NULL;printer->line_terminated=false;
    TextReader reader;reader_init(&reader,stream);
    char* line=NULL;size_t capacity=0,length,total=0,document_capacity=4096;
    char* document=(char*)rgx_malloc(document_capacity);
    bool binary=false;
    while(reader_line(&reader,&line,&capacity,&length)) {
        if(options->crlf && length && line[length-1]=='\r')--length;
        if(!options->text && memchr(line,0,length)){binary=true;break;}
        size_t additional=length+(reader.line_terminated?1:0);
        if(additional>64U*1024U*1024U-total){fprintf(stderr,"rgx: multiline input exceeds 64 MiB\n");rgx_had_error=true;break;}
        while(total+additional>document_capacity){document_capacity*=2;document=(char*)rgx_realloc_array(document,document_capacity,1);}
        if(length)memcpy(document+total,line,length);total+=length;if(reader.line_terminated)document[total++]='\n';
    }
    free(line);
    if(ferror(stream)){fprintf(stderr,"rgx: multiline read error: %s\n",path);rgx_had_error=true;}
    size_t starts_capacity=16;size_t* starts=(size_t*)rgx_malloc(sizeof(size_t)*starts_capacity);size_t line_count=1;starts[0]=0;
    for(size_t position=0;position<total;++position)if(document[position]=='\n' && position+1<total) {
        if(line_count==1048576){fprintf(stderr,"rgx: multiline line-map limit exceeded\n");rgx_had_error=true;break;}
        if(line_count+1==starts_capacity){starts_capacity*=2;starts=(size_t*)rgx_realloc_array(starts,starts_capacity,sizeof(size_t));}
        starts[line_count++]=position+1;
    }
    starts[line_count]=total;
    MatchSpan* spans=NULL;size_t span_count=0,span_capacity=0,offset=0;
    if(!binary && !(options->max_count_set && !options->max_count))while(offset<=total) {
        MatchSpan span;
        if(!matcher->vptr->FindMatch(matcher,document,total,offset,&span) || rgx_match_error)break;
        if(span_count==1048576){fprintf(stderr,"rgx: multiline match limit exceeded\n");rgx_had_error=true;break;}
        if(span_count==span_capacity){span_capacity=span_capacity?span_capacity*2:16;spans=(MatchSpan*)rgx_realloc_array(spans,span_capacity,sizeof(MatchSpan));}
        spans[span_count++]=span;
        if(options->quiet || options->files_with_matches || options->files_without_match)break;
        if(options->max_count_set && span_count>=options->max_count)break;
        offset=span.start+span.length;
        if(!span.length){if(offset==total)break;size_t width;utf8_rune(document+offset,total-offset,&width);offset+=width;}
    }
    bool print_path=options->filename_mode>=0?options->filename_mode!=0:multiple_targets;
    printer->bytes+=total;printer->file_bytes=total;printer->lines+=total?line_count:0;printer->file_lines=total?line_count:0;
    printer->matches+=span_count;printer->file_matches=span_count;
    if(options->files_without_match){if(!span_count){rgx_found=true;if(!options->quiet)RgxPrinter_PrintPath(printer,path);}}
    else if(span_count)rgx_found=true;
    unsigned char* selected_mask=(unsigned char*)rgx_calloc(line_count,1);
    int64_t* context_delta=(int64_t*)rgx_calloc(line_count+1,sizeof(int64_t));
    size_t selected_lines=0;
    for(size_t index=0;index<span_count;++index){size_t first=line_at(starts,line_count,spans[index].start),last=line_at(starts,line_count,spans[index].start+(spans[index].length?spans[index].length-1:0));
        for(size_t number=first;number<=last;++number)if(!selected_mask[number]){selected_mask[number]=1;++selected_lines;}
        size_t before=first>options->before_context?first-options->before_context:0;
        size_t after=options->after_context<line_count-1-last?last+options->after_context:line_count-1;
        ++context_delta[before];--context_delta[after+1];
    }
    if(!options->quiet && !options->files_without_match) {
        if(options->files_with_matches){if(span_count)RgxPrinter_PrintPath(printer,path);}
        else if((options->only_matching || options->json) && !options->count_only) {
            for(size_t index=0;index<span_count;++index) {
                MatchSpan span=spans[index];size_t first=line_at(starts,line_count,span.start);
                size_t last=line_at(starts,line_count,span.start+(span.length?span.length-1:0));
                size_t begin=starts[first],end=starts[last+1];if(end>begin && document[end-1]=='\n' && span.start+span.length<end)--end;
                printer->line_offset=begin;
                RgxPrinter_PrintMatch(printer,path,first+1,document+begin,end-begin,(MatchSpan){span.start-begin,span.length},print_path);
                if(rgx_match_error)break;
            }
        } else if(!options->count_only) {
            size_t last_printed=SIZE_MAX;
            size_t span_index=0;int64_t context_depth=0;
            for(size_t number=0;number<line_count;++number) {
                size_t begin=starts[number],end=starts[number+1];bool selected=false,context=false;MatchSpan local={0};
                if(end>begin && document[end-1]=='\n')--end;
                context_depth+=context_delta[number];context=context_depth>0;
                while(span_index<span_count && spans[span_index].start+(spans[span_index].length?spans[span_index].length-1:0)<begin)++span_index;
                if(span_index<span_count){MatchSpan span=spans[span_index];size_t first=line_at(starts,line_count,span.start),last=line_at(starts,line_count,span.start+(span.length?span.length-1:0));
                    if(number>=first && number<=last){selected=true;size_t low=span.start>begin?span.start:begin,high=span.start+span.length<end?span.start+span.length:end;local=(MatchSpan){low-begin,high>=low?high-low:0};}}
                if(selected || context) {
                    if(last_printed!=SIZE_MAX && number>last_printed+1)fprintf(printer->stream,"--\n");
                    printer->line_offset=begin;
                    if(selected)RgxPrinter_PrintMatch(printer,path,number+1,document+begin,end-begin,local,print_path);
                    else RgxPrinter_PrintContext(printer,path,number+1,document+begin,end-begin,print_path);
                    last_printed=number;
                }
                if(rgx_match_error)break;
            }
        }
    }
    printer->matched_lines+=selected_lines;printer->file_matched_lines=selected_lines;
    if(options->count_only && span_count && !options->quiet && !options->files_with_matches && !options->files_without_match)RgxPrinter_PrintCount(printer,path,options->count_matches || options->only_matching?span_count:selected_lines,print_path);
    printer_end_file(printer,path);free(selected_mask);free(context_delta);free(spans);free(starts);free(document);
}