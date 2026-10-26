#include "ignore_rules.h"
#include "directory_walker.h"
#include "allocation.h"
#include "search_state.h"
#include "path_conversion.h"
#include "text_reader.h"
#include "glob_matcher.h"

static void ignore_load_file(RgxWalker* walker, const char* scope, const char* filename) {
    char* path = (filename[0]=='/' || filename[0]=='\\' || (filename[0] && filename[1]==':'))?rgx_strdup(filename):path_join(scope, filename); if (!path) return;
    wchar_t* wide = path_to_wide(path); if (!wide) { free(path); return; }
    FILE* file = NULL; int error = _wfopen_s(&file, wide, L"rb"); free(wide);
    if (error || !file) { if (error != ENOENT && error != ENOTDIR) { fprintf(stderr, "rgx: cannot read ignore file: %s\n", path); rgx_had_error = true; } free(path); return; }
    TextReader reader; reader_init(&reader, file); char* line = NULL; size_t capacity = 0, length, bytes = 0;
    while (reader_line(&reader, &line, &capacity, &length)) {
        bytes += length;
        if (bytes > 1024U * 1024U || length > 8192) { fprintf(stderr, "rgx: ignore file exceeds safety limit: %s\n", path); rgx_had_error = true; break; }
        if (length && line[length - 1] == '\r') --length;
        while (length && line[length - 1] == ' ' && (length == 1 || line[length - 2] != '\\')) --length;
        if (!length || line[0] == '#') continue;
        if (memchr(line, 0, length)) { fprintf(stderr, "rgx: NUL byte in ignore file: %s\n", path); rgx_had_error = true; break; }
        size_t start = 0; bool negate = line[0] == '!'; if (negate) ++start;
        bool anchored = start < length && line[start] == '/'; if (anchored) ++start;
        bool directory_only = length > start && line[length - 1] == '/'; if (directory_only) --length;
        if (length <= start) continue;
        size_t allocation = strlen(scope) + 1 + length - start + 1 + sizeof(IgnoreRule);
        if (allocation > 16U * 1024U * 1024U - walker->rule_bytes) { fprintf(stderr, "rgx: ignore rules exceed 16 MiB safety limit\n"); rgx_had_error = true; break; }
        IgnoreRule* rule = (IgnoreRule*)rgx_calloc(1, sizeof(IgnoreRule)); rule->scope = rgx_strdup(scope);
        rule->pattern = (char*)rgx_malloc(length - start + 1); memcpy(rule->pattern, line + start, length - start); rule->pattern[length - start] = 0;
        rule->negate = negate; rule->anchored = anchored; rule->directory_only = directory_only; walker->rule_bytes += allocation;
        rule->priority=strcmp(filename,".rgignore")==0?3:strcmp(filename,".ignore")==0?2:strcmp(filename,".gitignore")==0?1:0;
        if (walker->tail) walker->tail->next = rule; else walker->rules = rule; walker->tail = rule;
    }
    if (ferror(file)) { fprintf(stderr, "rgx: error reading ignore file: %s\n", path); rgx_had_error = true; }
    free(line); fclose(file); free(path);
}

void ignore_load_directory(RgxWalker* walker, const char* scope) {
    if (walker->opts->no_ignore) return;
    for (LoadedScope* entry = walker->loaded; entry; entry = entry->next) {
        if (strcmp(entry->path, scope) == 0) return;
    }
    size_t cost = strlen(scope) + 1 + sizeof(LoadedScope);
    if (cost > 16U * 1024U * 1024U - walker->scope_bytes) {
        fprintf(stderr, "rgx: ignore scope memory limit exceeded\n");
        rgx_had_error = true;
        return;
    }
    LoadedScope* entry = (LoadedScope*)rgx_calloc(1, sizeof(LoadedScope));
    entry->path = rgx_strdup(scope);
    entry->next = walker->loaded;
    walker->loaded = entry;
    walker->scope_bytes += cost;
    bool git_scope=walker->opts->no_require_git || (walker->git_root &&
        _strnicmp(scope,walker->git_root,strlen(walker->git_root))==0 &&
        (scope[strlen(walker->git_root)]==0 || scope[strlen(walker->git_root)]=='/'));
    if(git_scope){ignore_load_file(walker, scope, ".git/info/exclude");ignore_load_file(walker, scope, ".gitignore");}
    ignore_load_file(walker, scope, ".ignore");
    ignore_load_file(walker, scope, ".rgignore");
}

static char* environment_path(const wchar_t* name) {
    DWORD needed=GetEnvironmentVariableW(name,NULL,0);if(!needed || needed>32768)return NULL;
    wchar_t* wide=(wchar_t*)rgx_calloc(needed,sizeof(wchar_t));
    if(!GetEnvironmentVariableW(name,wide,needed)){free(wide);return NULL;}
    char* path=wide_to_utf8(wide);free(wide);return path;
}

static bool global_git_config(RgxWalker* walker,const char* scope,const char* config,const char* home) {
    wchar_t* wide=path_to_wide(config);if(!wide)return false;
    FILE* file=NULL;int error=_wfopen_s(&file,wide,L"rb");free(wide);if(error || !file)return false;
    TextReader reader;reader_init(&reader,file);char* line=NULL;size_t capacity=0,length,consumed=0;bool core=false,found=false;
    while(reader_line(&reader,&line,&capacity,&length)) {
        consumed+=length;if(consumed>1024U*1024U || length>8192){rgx_had_error=true;break;}
        line=(char*)rgx_realloc_array(line,length+1,1);capacity=length+1;line[length]=0;
        char* begin=line;while(isspace((unsigned char)*begin))++begin;
        char* end=begin+strlen(begin);while(end>begin && isspace((unsigned char)end[-1]))*--end=0;
        if(*begin=='['){core=_stricmp(begin,"[core]")==0;continue;}
        if(!core || *begin=='#' || *begin==';')continue;
        char* equals=strchr(begin,'=');if(!equals)continue;*equals=0;
        end=equals;while(end>begin && isspace((unsigned char)end[-1]))*--end=0;
        if(_stricmp(begin,"excludesfile")!=0)continue;
        char* value=equals+1;while(isspace((unsigned char)*value))++value;
        size_t value_length=strlen(value);
        if(value_length>=2 && value[0]=='"' && value[value_length-1]=='"'){value[value_length-1]=0;++value;}
        if(!*value)continue;
        char* expanded=value[0]=='~' && (value[1]=='/' || value[1]=='\\') && home?path_join(home,value+2):rgx_strdup(value);
        if(expanded){ignore_load_file(walker,scope,expanded);free(expanded);found=true;}
    }
    if(ferror(file))rgx_had_error=true;free(line);fclose(file);return found;
}

static void global_git_ignores(RgxWalker* walker,const char* scope) {
    char* home=environment_path(L"USERPROFILE"),*xdg=environment_path(L"XDG_CONFIG_HOME"),*override=environment_path(L"GIT_CONFIG_GLOBAL");
    bool found=false;
    if(override)found=global_git_config(walker,scope,override,home);
    else if(home){char* config=path_join(home,".gitconfig");if(config){found=global_git_config(walker,scope,config,home);free(config);}}
    if(!found){char* ignore=xdg?path_join(xdg,"git/ignore"):home?path_join(home,".config/git/ignore"):NULL;if(ignore){ignore_load_file(walker,scope,ignore);free(ignore);}}
    free(home);free(xdg);free(override);
}

void ignore_load_ancestors(RgxWalker* walker, const char* directory) {
    if (walker->opts->no_ignore) return;
    free(walker->git_root);walker->git_root=NULL;
    char** scopes = NULL; size_t count = 0; char* current = rgx_strdup(directory);
    while (true) {
        if (count == 512) { fprintf(stderr, "rgx: ancestor depth exceeds safety limit\n"); rgx_had_error = true; break; }
        scopes = (char**)rgx_realloc_array(scopes, count + 1, sizeof(char*)); scopes[count++] = rgx_strdup(current);
        if(!walker->git_root){char* marker=path_join(current,".git");wchar_t* wide=marker?path_to_wide(marker):NULL;
            if(wide && GetFileAttributesW(wide)!=INVALID_FILE_ATTRIBUTES)walker->git_root=rgx_strdup(current);free(wide);free(marker);}
        char* slash = strrchr(current, '/');
        if (!slash || slash <= current + 2) break;
        *slash = 0;
    }
    if(walker->git_root || walker->opts->no_require_git)global_git_ignores(walker,walker->git_root?walker->git_root:directory);
    while (count) { ignore_load_directory(walker, scopes[--count]); free(scopes[count]); }
    free(scopes); free(current);
}

bool walker_ignored(RgxWalker* walker, const char* absolute, bool directory, const char* root) {
    bool ignored = false;
    int priority=-1;size_t deepest=0;
    for (IgnoreRule* rule = walker->rules; rule; rule = rule->next) {
        size_t scope_length = strlen(rule->scope);
        if (_strnicmp(absolute, rule->scope, scope_length) != 0 || absolute[scope_length] != '/') continue;
        if (rule->directory_only && !directory) continue;
        const char* relative = absolute + scope_length + 1;
        const char* base = strrchr(relative, '/'); if (base) ++base; else base = relative;
        const char* target = rule->anchored || strchr(rule->pattern, '/') ? relative : base;
        if (glob_match(rule->pattern, target) && (rule->priority>priority || (rule->priority==priority && scope_length>=deepest))) {
            ignored=!rule->negate;priority=rule->priority;deepest=scope_length;
        }
    }
    bool positive = false, included = false, negative = false;
    for (size_t index = 0; index < walker->opts->glob_count; ++index) {
        const char* pattern = walker->opts->globs[index]; bool exclude = *pattern == '!'; if (exclude) ++pattern; else positive = true;
        const char* target = strrchr(absolute, '/'); target = target ? target + 1 : absolute;
        if (strchr(pattern, '/')) { size_t root_length = strlen(root); target = strncmp(absolute, root, root_length) == 0 && absolute[root_length] == '/' ? absolute + root_length + 1 : absolute; }
        if (glob_match(pattern, target)) { included = !exclude; negative = exclude; ignored = exclude; }
    }
    if (negative) return true;
    if (positive && !directory && !included) return true;
    return directory && positive ? false : ignored;
}
