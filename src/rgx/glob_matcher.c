#include "glob_matcher.h"
#include "search_state.h"
#include "allocation.h"
#include "unicode.h"

typedef enum { G_LITERAL,G_ANY,G_STAR,G_DOUBLE,G_DIR_START,G_DIR_BODY,G_CLASS } GlobOp;
typedef struct { GlobOp op;uint32_t rune;const char* begin;const char* end;bool negate; } GlobToken;

static bool class_accepts(const GlobToken* token,uint32_t rune) {
    bool accepted=false;
    for(const char* cursor=token->begin;cursor<token->end;) {
        if(*cursor=='\\' && cursor+1<token->end)++cursor;
        size_t width;uint32_t low=utf8_rune(cursor,(size_t)(token->end-cursor),&width),high=low;cursor+=width;
        if(cursor+1<token->end && *cursor=='-') {
            ++cursor;if(*cursor=='\\' && cursor+1<token->end)++cursor;
            high=utf8_rune(cursor,(size_t)(token->end-cursor),&width);cursor+=width;
        }
        if(rune>=low && rune<=high)accepted=true;
    }
    return rune!='/' && (token->negate?!accepted:accepted);
}

static void glob_closure(const GlobToken* tokens,size_t count,unsigned char* states) {
    for(size_t index=0;index<count;++index)if(states[index]) {
        if(tokens[index].op==G_STAR || tokens[index].op==G_DOUBLE)states[index+1]=1;
        else if(tokens[index].op==G_DIR_START)states[index+2]=1;
    }
}

static bool glob_core(const char* pattern,const char* text,uint64_t* work) {
    size_t length=strlen(pattern),count=0;
    if(length>8192){rgx_had_error=true;fprintf(stderr,"rgx: glob exceeds 8192 bytes\n");return false;}
    GlobToken* tokens=(GlobToken*)rgx_calloc(length*2+1,sizeof(GlobToken));
    for(const char* cursor=pattern;*cursor;) {
        GlobToken token={0};
        if(*cursor=='*') {
            bool doubled=cursor[1]=='*';while(*cursor=='*')++cursor;token.op=doubled?G_DOUBLE:G_STAR;
            if(doubled && *cursor=='/'){++cursor;token.op=G_DIR_START;tokens[count++]=token;token.op=G_DIR_BODY;}
        }else if(*cursor=='?'){++cursor;token.op=G_ANY;}
        else if(*cursor=='[') {
            ++cursor;token.op=G_CLASS;token.negate=*cursor=='!' || *cursor=='^';if(token.negate)++cursor;
            token.begin=cursor;if(*cursor==']')++cursor;
            while(*cursor && *cursor!=']'){if(*cursor=='\\' && cursor[1])++cursor;++cursor;}
            if(!*cursor){free(tokens);rgx_had_error=true;fprintf(stderr,"rgx: unclosed glob class\n");return false;}
            token.end=cursor++;
        }else {
            if(*cursor=='\\' && cursor[1])++cursor;
            size_t width;token.rune=utf8_rune(cursor,strlen(cursor),&width);cursor+=width;token.op=G_LITERAL;
        }
        tokens[count++]=token;
    }
    unsigned char* current=(unsigned char*)rgx_calloc(count+1,1);
    unsigned char* next=(unsigned char*)rgx_calloc(count+1,1);
    current[0]=1;glob_closure(tokens,count,current);
    for(size_t position=0,text_length=strlen(text);position<text_length;) {
        size_t width;uint32_t rune=utf8_rune(text+position,text_length-position,&width);position+=width;memset(next,0,count+1);
        for(size_t index=0;index<count;++index)if(current[index]) {
            if(++*work>1000000){rgx_had_error=true;fprintf(stderr,"rgx: glob work limit exceeded\n");free(tokens);free(current);free(next);return false;}
            GlobToken token=tokens[index];
            if(token.op==G_STAR){if(rune!='/')next[index]=1;}
            else if(token.op==G_DOUBLE)next[index]=1;
            else if(token.op==G_DIR_START)next[index+1]=1;
            else if(token.op==G_DIR_BODY)next[rune=='/'?index-1:index]=1;
            else if(token.op==G_ANY?rune!='/':token.op==G_CLASS?class_accepts(&token,rune):rune==token.rune)next[index+1]=1;
        }
        unsigned char* swap=current;current=next;next=swap;glob_closure(tokens,count,current);
    }
    bool accepted=current[count]!=0;free(tokens);free(current);free(next);return accepted;
}

static bool glob_alternatives(const char* pattern,const char* text,unsigned depth,uint64_t* work) {
    if(depth>8 || strlen(pattern)>8192){rgx_had_error=true;fprintf(stderr,"rgx: glob expansion limit exceeded\n");return false;}
    const char* open=NULL;
    for(const char* cursor=pattern;*cursor;++cursor){if(*cursor=='\\' && cursor[1])++cursor;else if(*cursor=='{'){open=cursor;break;}}
    if(!open)return glob_core(pattern,text,work);
    unsigned nested=0;const char* close=open+1;
    for(;*close;++close){if(*close=='\\' && close[1])++close;else if(*close=='{')++nested;else if(*close=='}'){if(!nested)break;--nested;}}
    if(!*close){rgx_had_error=true;fprintf(stderr,"rgx: unclosed glob alternatives\n");return false;}
    const char* segment=open+1;nested=0;
    for(const char* cursor=segment;cursor<=close;++cursor) {
        if(cursor<close && *cursor=='\\' && cursor+1<close){++cursor;continue;}
        if(cursor<close && *cursor=='{')++nested;
        if(cursor<close && *cursor=='}')--nested;
        if(cursor==close || (*cursor==',' && !nested)) {
            size_t prefix=(size_t)(open-pattern),part=(size_t)(cursor-segment),suffix=strlen(close+1);
            char* expanded=(char*)rgx_malloc(prefix+part+suffix+1);
            memcpy(expanded,pattern,prefix);memcpy(expanded+prefix,segment,part);memcpy(expanded+prefix+part,close+1,suffix+1);
            bool accepted=glob_alternatives(expanded,text,depth+1,work);free(expanded);if(accepted)return true;
            if(rgx_had_error)return false;
            segment=cursor+1;if(++*work>1000000){rgx_had_error=true;return false;}
        }
    }
    return false;
}

bool glob_match(const char* pattern,const char* text) {
    uint64_t work=0;return glob_alternatives(pattern,text,0,&work);
}
