/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
@author: Xuanlong
@email:<xuanlong@pku.edu.cn>
@time: 2017/06/01
*/
#include "app.h"
#include "agent.h"
#include <iostream>//qxl

static class ApplicationClass : public TclClass {
 public:
	ApplicationClass() : TclClass("Application") {}
	TclObject* create(int, const char*const*) {
		return (new Application);
	}
} class_application;


Application::Application() : enableRecv_(0), enableResume_(0), 
stopAndCopy(0), dirtyRate(0.02), 
vmThd(1460), migrationTurn(0)
{//qxl
	bind("dirtyRate", &dirtyRate);
	bind("vmThd", &vmThd);
	bind("vmSize",&vmSize);
	bind("m_totalSend",&m_totalSend);
	bind("m_totalTime",&m_totalTime);

}


int Application::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();

	if (argc == 2) {
		if (strcmp(argv[1], "start") == 0) {
			// enableRecv_ only if recv() exists in Tcl
			tcl.evalf("[%s info class] info instprocs", name_);
			char result[1024];
			sprintf(result, " %s ", tcl.result());
			enableRecv_ = (strstr(result, " recv ") != 0);
			enableResume_ = (strstr(result, " resume ") != 0);
			start();
			return (TCL_OK);
		}
		if (strcmp(argv[1], "stop") == 0) {
			stop();
			return (TCL_OK);
		}
		if (strcmp(argv[1], "agent") == 0) {
			tcl.resultf("%s", agent_->name());
			return (TCL_OK);
		}
		if (strcmp(argv[1], "getTime") == 0) {
			tcl.resultf("%s", agent_->name());
			return (TCL_OK);
		}
	}
	else if (argc == 3) {
		if (strcmp(argv[1], "attach-agent") == 0) {
			agent_ = (Agent*) TclObject::lookup(argv[2]);
			if (agent_ == 0) {
				tcl.resultf("no such agent %s", argv[2]);
				return(TCL_ERROR);
			}
			agent_->attachApp(this);
			return(TCL_OK);
		}
		if (strcmp(argv[1], "send") == 0) {
			send(atoi(argv[2]));
			return(TCL_OK);
		}
	}
	return (Process::command(argc, argv));
}


void Application::start()
{
}


void Application::stop()
{
}


void Application::send(int nbytes)
{
	agent_->sendmsg(nbytes);
}


void Application::recv(int nbytes)
{
	if (! enableRecv_)
		return;
	Tcl& tcl = Tcl::instance();
	tcl.evalf("%s recv %d", name_, nbytes);
}


void Application::resume()
{
	if (! enableResume_)
		return;
	Tcl& tcl = Tcl::instance();
	tcl.evalf("%s resume", name_);
}

//qxl
void Application::processVM(int dataDelta, double timeDelta)
{
	if (migrationTurn == 0){
		migrationTurn = 1;
	}

	static int iteration = 0;
	std::cout << "dataDelata:\t" << dataDelta << "\ttimeDelta:\t" << timeDelta << endl;
	cout << "iteration:" << iteration << endl;
	p = 0.3586 - (0.0463*timeDelta) - (0.0001*dirtyRate/1048576);//time sec,dirty Mb/s
	WWS = p*timeDelta*dirtyRate;
	m_nextSend = timeDelta*dirtyRate - WWS;
	iteration = iteration + 1;
	dataDelta = m_nextSend;

	m_totalSend += dataDelta;
	m_totalTime += timeDelta;

	if ((m_nextSend < vmThd)||(iteration > 30)||(m_totalSend > 3*vmSize))
	{
		cout << "vmSize:"<< vmSize << endl;
		cout << "m_nextSend:" << m_nextSend << endl;
		cout << "m_totalSend:" << (m_totalSend + vmSize)/1000000 << "M"<< endl;
		cout << "m_totalTime:" << m_totalTime <<  endl;

		stopAndCopy = 1;
	}
	agent_->sendmsg(m_nextSend);

}
