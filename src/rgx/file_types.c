#include "file_types.h"
#include "glob_matcher.h"
#include "allocation.h"

static const struct {const char* name;const char* glob;} builtins[]={
    {"c","*.{c,h}"},{"cpp","*.{cpp,cxx,cc,hpp,hxx,hh,C,H}"},{"csharp","*.cs"},
    {"css","*.{css,scss,sass}"},{"go","*.go"},{"html","*.{html,htm}"},
    {"java","*.java"},{"js","*.{js,jsx,mjs,cjs}"},{"json","*.{json,jsonl}"},
    {"kotlin","*.{kt,kts}"},{"make","{Makefile,makefile,*.mk}"},{"markdown","*.{md,markdown}"},
    {"php","*.php"},{"powershell","*.{ps1,psm1,psd1}"},{"py","*.{py,pyi,pyw}"},
    {"ruby","*.{rb,rake}"},{"rust","*.rs"},{"sh","*.{sh,ksh,bash,zsh}"},
    {"sql","*.sql"},{"swift","*.swift"},{"text","*.{txt,log}"},
    {"toml","*.toml"},{"ts","*.{ts,tsx}"},{"xml","*.{xml,xsd,svg}"},
    {"yaml","*.{yaml,yml}"},{"batch","*.{bat,cmd}"}
};

static bool cleared(const RgxOptions* options,const char* name) {
    for(size_t index=0;index<options->type_clear_count;++index)
        if(strcmp(options->type_clear[index],name)==0 || strcmp(options->type_clear[index],"all")==0)return true;
    return false;
}
static bool custom_name(const char* definition,const char* name) {
    const char* colon=strchr(definition,':');
    return colon && (size_t)(colon-definition)==strlen(name) && strncmp(definition,name,(size_t)(colon-definition))==0;
}
static bool known(const RgxOptions* options,const char* name) {
    if(strcmp(name,"all")==0)return true;
    for(size_t index=0;index<sizeof(builtins)/sizeof(builtins[0]);++index)
        if(strcmp(name,builtins[index].name)==0 && !cleared(options,name))return true;
    for(size_t index=0;index<options->type_add_count;++index)if(custom_name(options->type_add[index],name))return true;
    return false;
}
bool file_types_validate(const RgxOptions* options) {
    for(size_t index=0;index<options->type_add_count;++index) {
        const char* definition=options->type_add[index];const char* colon=strchr(definition,':');
        if(!colon || colon==definition || !colon[1] || (size_t)(colon-definition)>64){fprintf(stderr,"rgx: invalid type definition: %s\n",definition);return false;}
        for(const char* cursor=definition;cursor<colon;++cursor)if(!isalnum((unsigned char)*cursor) && *cursor!='_'){fprintf(stderr,"rgx: invalid type name\n");return false;}
    }
    for(size_t group=0;group<2;++group) {
        char** names=group?options->type_not:options->types;size_t count=group?options->type_not_count:options->type_count;
        for(size_t index=0;index<count;++index)if(!known(options,names[index])){fprintf(stderr,"rgx: unknown file type: %s\n",names[index]);return false;}
    }
    return true;
}
static bool matches(const RgxOptions* options,const char* name,const char* base) {
    bool all=strcmp(name,"all")==0;
    for(size_t index=0;index<sizeof(builtins)/sizeof(builtins[0]);++index)
        if((all || strcmp(name,builtins[index].name)==0) && !cleared(options,builtins[index].name) && glob_match(builtins[index].glob,base))return true;
    for(size_t index=0;index<options->type_add_count;++index) {
        const char* definition=options->type_add[index];
        if((all || custom_name(definition,name)) && glob_match(strchr(definition,':')+1,base))return true;
    }
    return false;
}
bool file_type_selected(const RgxOptions* options,const char* path) {
    const char* base=path;for(const char* cursor=path;*cursor;++cursor)if(*cursor=='/' || *cursor=='\\')base=cursor+1;
    bool included=!options->type_count;
    for(size_t index=0;index<options->type_count;++index)if(matches(options,options->types[index],base))included=true;
    for(size_t index=0;index<options->type_not_count;++index)if(matches(options,options->type_not[index],base))return false;
    return included;
}
void file_types_list(const RgxOptions* options) {
    for(size_t index=0;index<sizeof(builtins)/sizeof(builtins[0]);++index)
        if(!cleared(options,builtins[index].name))printf("%s: %s\n",builtins[index].name,builtins[index].glob);
    for(size_t index=0;index<options->type_add_count;++index)puts(options->type_add[index]);
}