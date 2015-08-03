#include "StreamParser.h"

StreamParser::StreamParser(string filepath, ObsQueue* out, MsgQueue* msg):
	m_filepath(filepath),
	m_streambuf(filepath),
	m_queue(out),
	m_msgQueue(msg)
{}

void StreamParser::run(){
	try{
		parse();
	} catch(exception& e){
		m_queue->setQueueEnd();
		throw e;
	}
}

void StreamParser::parse(){
	Counter counter;
	char line[128];
	char* cells[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	istream mstream(&m_streambuf);

	counter.registerCallback([&](uint64_t countPerSec){
		string msg = to_string(countPerSec) + " line/s";
		if(m_queue->full()) msg += "\tblocked!";
		m_msgQueue->enqueue(Message(PARSER, msg));
	});
	
	mstream.seekg(30); //skip headers
	counter.start();

	try{
		while(!mstream.eof()){
			mstream.getline(line, 128);
			if(mstream.eof() || mstream.fail()){
				if(mstream.get() == EOF)
					break;
				else
					throw std::exception("Parse error (line)");
			}
			for(unsigned short i = 0; i < 6; i++){
				cells[i] = (i == 0) ? strtok(line, ",") : strtok(NULL, ",");
				if(cells[i] == nullptr)
					throw std::exception("Parse error (tokens)");
			}
			const float* bid = new float(atof(cells[3]));
			const float* offer = new float(atof(cells[4]));
			const unsigned short* mode = new const unsigned short(atoi(cells[5]));
			if(*bid > 0.01 && *offer > 0.01 && *mode == 12){
				const string* symbol = new string(cells[0]);
				const string* date = new string(cells[1]);
				const string* time = new string(cells[2]);
				if(!symbol->empty() && !date->empty() && !time->empty()){
					Observation obs(
						symbol,
						date,
						time,
						offer,
						bid,
						mode
						);
					m_queue->enqueue(move(obs));
					counter.tick();
				} else{
					delete symbol; delete date; delete time;
					symbol = nullptr; date = nullptr; time = nullptr;
					throw std::exception("Parse error (strings)");
				}
			} else{
				delete offer;
				delete bid;
				delete mode;
			}
		}
		m_queue->setQueueEnd();
		counter.stop();
		m_msgQueue->enqueue(Message(PARSER, "Finished !"));
	} catch(exception& e){
		m_queue->setQueueEnd();
		counter.stop();
		m_msgQueue->enqueue(Message(PARSER, string("Exception: ") + e.what()));
	}
}