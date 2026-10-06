#include "output_buffer.hpp"

void OutputBuffer::emit() {if(pending_.empty())return;std::string out;if(format_==OutputFormat::Json){if(!first_)target_->sputn(",\n",2);first_=false;out="{\"message\":\"";for(char c:pending_){if(c=='"'||c=='\\')out+='\\';if(c=='\r')out+="\\r";else out+=c;}out+="\"}";}else if(format_==OutputFormat::Csv){out="\"";for(char c:pending_)out+=c=='"'?"\"\"":std::string(1,c);out+="\"\n";}else out=pending_+"\n";target_->sputn(out.data(),static_cast<std::streamsize>(out.size()));pending_.clear();}

OutputBuffer::OutputBuffer(std::streambuf*t,OutputFormat f):target_(t),format_(f) {if(f==OutputFormat::Json)target_->sputn("[\n",2);else if(f==OutputFormat::Table)target_->sputn("MESSAGE\n-------\n",16);}

OutputBuffer::~OutputBuffer() {emit();if(format_==OutputFormat::Json)target_->sputn("\n]\n",3);}

OutputBuffer::int_type OutputBuffer::overflow(int_type c) {if(c!=traits_type::eof()){if(c=='\n')emit();else pending_+=static_cast<char>(c);}return traits_type::not_eof(c);}

int OutputBuffer::sync() {emit();return target_->pubsync();}
