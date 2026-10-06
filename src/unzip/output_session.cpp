#include "output_buffer.hpp"
#include "output_session.hpp"
#include "pipe_buffer.hpp"

OutputSession::OutputSession(OutputFormat f,const std::string&cmd):old_(std::cout.rdbuf()) {if(f==OutputFormat::Human&&cmd.empty())return;std::streambuf*t=old_;if(!cmd.empty()&&(file_=_popen(cmd.c_str(),"w"))){pipe_=new PipeBuffer(file_);t=pipe_;}out_=new OutputBuffer(t,f);std::cout.rdbuf(out_);}

OutputSession::~OutputSession() {if(!out_)return;std::cout.flush();std::cout.rdbuf(old_);delete out_;delete pipe_;if(file_)_pclose(file_);}
