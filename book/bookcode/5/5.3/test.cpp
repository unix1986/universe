#include <iostream>
#include <sstream>
#include "MessageHead.h"

using namespace std;

class CLFatherWordCountObserver : public CLMessageObserver
{
public:
	CLFatherWordCountObserver()
	{
		m_Child = NULL;
	}

	virtual ~CLFatherWordCountObserver()
	{
		for(int i = 0; i < m_process_num; i++)
		   if(m_Child[i] != NULL)
			   m_Child[i]->WaitForDeath();

		delete[] m_Child;
	}
    
	void PushDirname(char *dirname)
	{
        m_dirlist.push_back(dirname);
	}
    
	void Init( )
	{
	    m_process_num = m_dirlist.size();

		m_Child = new CLExecutive *[m_process_num];
		for(int i = 0; i < m_process_num; i++)
		   m_Child[i] = NULL;

		m_quit_count = 0;
		m_dir_num = 0;
	}

	virtual CLStatus Initialize(CLMessageLoopManager *pMessageLoop, void* pContext)
	{
        pMessageLoop->Register(CHILD_INIT_MESSAGE_ID, (CallBackForMessageLoop)(&CLFatherWordCountObserver::On_Child_Init));
		pMessageLoop->Register(CHILD_SEND_REQUEST_MESSAGE_ID, (CallBackForMessageLoop)(&CLFatherWordCountObserver::On_Child_Send_Request));
		pMessageLoop->Register(INTERMEDIATE_RESULT_MESSAGE_ID, (CallBackForMessageLoop)(&CLFatherWordCountObserver::On_Intermediate_Result));
        pMessageLoop->Register(CHILD_WORK_FINISH_MESSAGE_ID, (CallBackForMessageLoop)(&CLFatherWordCountObserver::On_Child_Work_Finish));
		
		for(int i = 0;i < m_process_num; i++)
		{  	
		    stringstream t_index;
			t_index << i+1;
			string runcmd = string("./test/a.out child_pipe") + t_index.str();
			
			m_Child[i] = new CLProcess(new CLProcessFunctionForExec, true);
		    if(!((m_Child[i]->Run((void *)(runcmd.c_str( )))).IsSuccess()))
	     	{
			    cout << "Run error" << endl;
			    m_Child[i] = NULL;
	    	}
		}  
        
		return CLStatus(0, 0);
	}

	CLStatus On_Child_Init(CLMessage *pm)
	{
		CLChildInitMsg *p = dynamic_cast<CLChildInitMsg*>(pm);
		if(p == 0)
			return CLStatus(0,0);
        
		string childname = p->childname;
		cout << "Father :: receive child "<< childname << " init msg ok !!!!!!!!!!!!!!!" << endl << endl;
		
		CLSharedExecutiveCommunicationByNamedPipe *pSender = new CLSharedExecutiveCommunicationByNamedPipe(childname.c_str());
	    pSender->RegisterSerializer(FATHER_INIT_MESSAGE_ID,new CLFatherInitMsgSerializer);
		pSender->RegisterSerializer(FATHER_ACK_MESSAGE_ID,new CLFatherAckMsgSerializer);
        pSender->RegisterSerializer(QUIT_MESSAGE_ID,new CLQuitMsgSerializer);
        
	    CLExecutiveNameServer::GetInstance()->Register(childname.c_str(),pSender);
	 
	    CLFatherInitMsg *pFatherInitMsg = new CLFatherInitMsg;
	    pFatherInitMsg->dirname = m_dirlist[m_dir_num ++];
        cout << "Father :: send init msg to " << childname << " !!!!!!!!!!!!!!!!" << endl << endl;	
	    CLExecutiveNameServer::PostExecutiveMessage(childname.c_str(),pFatherInitMsg);
		
		return CLStatus(0,0);
	}
	
	CLStatus On_Child_Send_Request(CLMessage *pm)
	{
		CLChildSendRequestMsg *p = dynamic_cast<CLChildSendRequestMsg *>(pm);
		if(p == 0)
			return CLStatus(0,0);

        string childname = p->childname;
		cout << "Father :: receive " << childname.c_str() << " send request ok !!!!!!!!!!!!!!!!!" << endl << endl;
        
		CLFatherAckMsg *pFatherAckMsg = new CLFatherAckMsg;
        cout << "Father ::  send ack msg to " << childname << " !!!!!!!!!!!!!!!!!" << endl << endl;
		CLExecutiveNameServer::PostExecutiveMessage(childname.c_str(),pFatherAckMsg);
		
		return CLStatus(0,0);
	}
	
	CLStatus On_Intermediate_Result(CLMessage *pm)
	{
		CLIntermediateResultMsg *p = dynamic_cast<CLIntermediateResultMsg*>(pm);
		if(p == 0)
			return CLStatus(0, 0);
		
		string word = p->data.word;
	    unsigned int count = p->data.count;
        
		iter = word_table.find(word);
		if(iter == word_table.end())
			word_table.insert(pair<string,unsigned int>(word,count));
		else
			iter->second += count;
         
		return CLStatus(0,0);
	}

	CLStatus On_Child_Work_Finish(CLMessage *pm)
	{
		CLChildWorkFinishMsg *p = dynamic_cast<CLChildWorkFinishMsg *>(pm);
		if(p == 0)
			return CLStatus(0,0);
		
		string childname = p->childname;
		cout << "Father :: receive child "<< childname <<" work finish msg ok !!!!!!!!!!!!!!!" << endl << endl;
		CLQuitMsg *pQuitMsg = new CLQuitMsg;
	
		CLExecutiveNameServer::PostExecutiveMessage(childname.c_str(),pQuitMsg);
        CLExecutiveNameServer::GetInstance()->ReleaseCommunicationPtr(childname.c_str());
         
		if((++m_quit_count) < m_process_num)
			return CLStatus(0,0);

		fstream outfile;
		outfile.open("./result",fstream::out);
		
		for(iter = word_table.begin(); iter != word_table.end(); iter++)
        {
            outfile << iter->first;
            outfile << "\t";
            outfile << iter->second;
            outfile << "\n";
        }
			
		outfile.close();
	    
		cout << "Father :: work done,quit !!!!!!!!!!!!!!!!!!!" << endl << endl;
		
		return CLStatus(QUIT_MESSAGE_LOOP,0);
	}

private:
	CLExecutive **m_Child;
	vector<string> m_dirlist;
	map<string,unsigned int> word_table;
	map<string,unsigned int>::iterator iter;
	unsigned short m_process_num;
    unsigned short m_quit_count;
    unsigned short m_dir_num;
};

int main(int argc,char **argv)
{
	if(argc < 2)
	{
		cout << "usage:./a.out [dirname1] [dirname2] ... " <<endl;
		exit(-1);
	}

	try
	{
		if(!CLLibExecutiveInitializer::Initialize().IsSuccess())
		{
			cout << "Initialize error" << endl;
			return 0;
		}

		CLFatherWordCountObserver *pFatherWordCountObserver = new CLFatherWordCountObserver;
	
		for(int i = 1;i < argc; i++)
			pFatherWordCountObserver->PushDirname(argv[i]);
		pFatherWordCountObserver->Init();

		CLNonThreadForMsgLoop father_nonthread(pFatherWordCountObserver, "father_pipe", EXECUTIVE_BETWEEN_PROCESS_USE_PIPE_QUEUE);
		father_nonthread.RegisterDeserializer(CHILD_INIT_MESSAGE_ID, new CLChildInitMsgDeserializer);
        father_nonthread.RegisterDeserializer(CHILD_SEND_REQUEST_MESSAGE_ID, new CLChildSendRequestMsgDeserializer);
		father_nonthread.RegisterDeserializer(INTERMEDIATE_RESULT_MESSAGE_ID, new CLIntermediateResultMsgDeserializer);
		father_nonthread.RegisterDeserializer(CHILD_WORK_FINISH_MESSAGE_ID, new CLChildWorkFinishMsgDeserializer);
        
		father_nonthread.Run(0);

		throw CLStatus(0, 0);
	}
	catch(CLStatus& s)
	{
		if(!CLLibExecutiveInitializer::Destroy().IsSuccess())
			cout << "Destroy error" << endl;

		return 0;
	}
}
