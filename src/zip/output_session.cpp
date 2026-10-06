#include "output_buffer.hpp"
#include "output_session.hpp"
#include "pipe_buffer.hpp"

OutputSession::OutputSession(OutputFormat f,const std::string& cmd):old_(std::cout.rdbuf()) {if(f==OutputFormat::Human&&cmd.empty())return;std::streambuf* target=old_;if(!cmd.empty()&&(file_=_popen(cmd.c_str(),"w"))){pipe_=new PipeBuffer(file_);target=pipe_;}output_=new OutputBuffer(target,f);std::cout.rdbuf(output_);}

OutputSession::~OutputSession() {if(!output_)return;std::cout.flush();std::cout.rdbuf(old_);delete output_;delete pipe_;if(file_)_pclose(file_);}
